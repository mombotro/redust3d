#include "rig_generator.h"
#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <dust3d/base/debug.h>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Fraction of bone radius used as the gradient blend region at each bone junction.
static const float kBeginGradientRadiusScale = 0.5f;
static const float kEndGradientRadiusScale = 0.5f;

// Weight assigned to each bone at the midpoint of a gradient blend region.
static const float kJunctionBlendWeight = 0.5f;

// Scale factor for averaging two direction vectors at a bone junction.
static const float kDirectionAverageScale = 0.5f;

// Helper: angle in radians between two vectors.
static float radianBetweenVectors(const dust3d::Vector3& first, const dust3d::Vector3& second)
{
    double cosA = dust3d::Vector3::dotProduct(first.normalized(), second.normalized());
    cosA = std::max(-1.0, std::min(1.0, cosA));
    return (float)std::acos(cosA);
}

// Helper: bone mark color (approximates RC6 BoneMarkToColor).
static dust3d::Color boneMarkToColor(dust3d::BoneMark mark)
{
    switch (mark) {
    case dust3d::BoneMark::Neck:
        return dust3d::Color(0xfd / 255.0, 0x64 / 255.0, 0x61 / 255.0);
    case dust3d::BoneMark::Limb:
        return dust3d::Color(0x55 / 255.0, 0xad / 255.0, 0xff / 255.0);
    case dust3d::BoneMark::Tail:
        return dust3d::Color(0x4c / 255.0, 0xaf / 255.0, 0x50 / 255.0);
    case dust3d::BoneMark::Joint:
        return dust3d::Color(0xff / 255.0, 0xe0 / 255.0, 0x82 / 255.0);
    default:
        return dust3d::Color(1.0, 1.0, 1.0);
    }
}

static dust3d::Color colorWhite()
{
    return dust3d::Color(1.0, 1.0, 1.0);
}

RigGenerator::RigGenerator(dust3d::RigType rigType, const dust3d::Object& object)
    : m_rigType(rigType)
    , m_object(&object)
{
}

RigGenerator::~RigGenerator()
{
    delete m_resultBones;
    delete m_resultWeights;
}

std::vector<dust3d::RiggerBone>* RigGenerator::takeResultBones()
{
    std::vector<dust3d::RiggerBone>* resultBones = m_resultBones;
    m_resultBones = nullptr;
    return resultBones;
}

std::map<int, dust3d::RiggerVertexWeights>* RigGenerator::takeResultWeights()
{
    std::map<int, dust3d::RiggerVertexWeights>* resultWeights = m_resultWeights;
    m_resultWeights = nullptr;
    return resultWeights;
}

bool RigGenerator::isSuccessful() const
{
    return m_isSuccessful;
}

void RigGenerator::groupNodeIndices(const std::map<size_t, std::unordered_set<size_t>>& neighborMap,
    std::vector<std::unordered_set<size_t>>* groups)
{
    std::unordered_set<size_t> visited;
    for (const auto& it : neighborMap) {
        if (visited.find(it.first) != visited.end())
            continue;
        std::unordered_set<size_t> group;
        std::queue<size_t> waitQueue;
        visited.insert(it.first);
        group.insert(it.first);
        waitQueue.push(it.first);
        while (!waitQueue.empty()) {
            auto nodeIndex = waitQueue.front();
            waitQueue.pop();
            auto findNeighbor = neighborMap.find(nodeIndex);
            if (findNeighbor != neighborMap.end()) {
                for (const auto& neighbor : findNeighbor->second) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);
                        group.insert(neighbor);
                        waitQueue.push(neighbor);
                    }
                }
            }
        }
        groups->push_back(group);
    }
}

void RigGenerator::buildNeighborMap()
{
    // Build m_bodyNodes from m_object->nodeMap
    m_bodyNodes.clear();
    m_bodyNodeIdToIndex.clear();
    for (const auto& kv : m_object->nodeMap) {
        size_t idx = m_bodyNodes.size();
        m_bodyNodeIdToIndex[kv.first] = idx;
        BodyNode bn;
        bn.nodeId = kv.first;
        bn.origin = kv.second.origin;
        bn.radius = kv.second.radius;
        bn.boneMark = kv.second.boneMark;
        m_bodyNodes.push_back(bn);
    }

    // Build neighbor map from m_object->bodyEdges
    for (size_t i = 0; i < m_bodyNodes.size(); ++i)
        m_neighborMap[i] = {};

    for (const auto& edge : m_object->bodyEdges) {
        auto fromIt = m_bodyNodeIdToIndex.find(edge.first);
        auto toIt = m_bodyNodeIdToIndex.find(edge.second);
        if (fromIt == m_bodyNodeIdToIndex.end() || toIt == m_bodyNodeIdToIndex.end())
            continue;
        m_neighborMap[fromIt->second].insert(toIt->second);
        m_neighborMap[toIt->second].insert(fromIt->second);
    }

    while (true) {
        std::vector<std::unordered_set<size_t>> groups;
        groupNodeIndices(m_neighborMap, &groups);
        if (groups.size() < 2)
            break;

        std::vector<std::pair<size_t, size_t>> groupEndpoints;
        for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
            const auto& group = groups[groupIndex];
            for (const auto& nodeIndex : group) {
                if (m_neighborMap[nodeIndex].size() >= 2)
                    continue;
                groupEndpoints.push_back({ groupIndex, nodeIndex });
            }
        }

        if (groupEndpoints.empty())
            break;

        std::vector<std::pair<size_t, float>> stitchResult(groupEndpoints.size(),
            { m_bodyNodes.size(), std::numeric_limits<float>::max() });

        for (size_t i = 0; i < groupEndpoints.size(); ++i) {
            const auto fromGroupIndex = groupEndpoints[i].first;
            const auto fromNodeIndex = groupEndpoints[i].second;
            size_t bestMatch = m_bodyNodes.size();
            float bestDist2 = std::numeric_limits<float>::max();
            for (size_t j = 0; j < groupEndpoints.size(); ++j) {
                if (groupEndpoints[j].first == fromGroupIndex)
                    continue;
                const auto toNodeIndex = groupEndpoints[j].second;
                const auto& a = m_bodyNodes[fromNodeIndex].origin;
                const auto& b = m_bodyNodes[toNodeIndex].origin;
                float dx = (float)(a.x() - b.x()), dy = (float)(a.y() - b.y()), dz = (float)(a.z() - b.z());
                float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestMatch = toNodeIndex;
                }
            }
            stitchResult[i] = { bestMatch, bestDist2 };
        }

        auto minDistantMatch = std::min_element(stitchResult.begin(), stitchResult.end(),
            [&](const std::pair<size_t, float>& first,
                const std::pair<size_t, float>& second) {
                return first.second < second.second;
            });
        if (minDistantMatch->first == m_bodyNodes.size())
            break;

        const auto& fromNodeIndex = groupEndpoints[minDistantMatch - stitchResult.begin()].second;
        const auto& toNodeIndex = minDistantMatch->first;
        m_neighborMap[fromNodeIndex].insert(toNodeIndex);
        m_neighborMap[toNodeIndex].insert(fromNodeIndex);
    }
}

void RigGenerator::buildBoneNodeChain()
{
    std::vector<std::tuple<size_t, std::unordered_set<size_t>, bool>> segments;
    std::unordered_set<size_t> middle;
    size_t middleStartNodeIndex = m_bodyNodes.size();
    for (size_t nodeIndex = 0; nodeIndex < m_bodyNodes.size(); ++nodeIndex) {
        const auto& node = m_bodyNodes[nodeIndex];
        if (!dust3d::boneMarkIsBranchNode(node.boneMark))
            continue;
        m_branchNodesMapByMark[(int)node.boneMark].push_back(nodeIndex);
        if (dust3d::BoneMark::Neck == node.boneMark) {
            if (middleStartNodeIndex == m_bodyNodes.size())
                middleStartNodeIndex = nodeIndex;
        } else if (dust3d::BoneMark::Tail == node.boneMark) {
            middleStartNodeIndex = nodeIndex;
        }
        std::unordered_set<size_t> left;
        std::unordered_set<size_t> right;
        splitByNodeIndex(nodeIndex, &left, &right);
        if (left.size() > right.size())
            std::swap(left, right);
        for (const auto& it : right)
            middle.insert(it);
        segments.push_back(std::make_tuple(nodeIndex, left, false));
    }
    for (const auto& it : segments) {
        const auto& nodeIndex = std::get<0>(it);
        const auto& left = std::get<1>(it);
        for (const auto& item : left)
            middle.erase(item);
        middle.erase(nodeIndex);
    }
    middle.erase(middleStartNodeIndex);
    if (middleStartNodeIndex != m_bodyNodes.size())
        segments.push_back(std::make_tuple(middleStartNodeIndex, middle, true));
    for (const auto& it : segments) {
        const auto& fromNodeIndex = std::get<0>(it);
        const auto& left = std::get<1>(it);
        const auto& isSpine = std::get<2>(it);
        const auto& fromNode = m_bodyNodes[fromNodeIndex];
        std::vector<std::vector<size_t>> boneNodeIndices;
        std::unordered_set<size_t> visited;
        size_t attachNodeIndex = fromNodeIndex;
        collectNodesForBoneRecursively(fromNodeIndex,
            &left,
            &boneNodeIndices,
            0,
            &visited);
        if (dust3d::BoneMark::Limb == fromNode.boneMark) {
            for (const auto& neighbor : m_neighborMap[fromNodeIndex]) {
                if (left.find(neighbor) == left.end()) {
                    attachNodeIndex = neighbor;
                    break;
                }
            }
        }
        m_boneNodeChain.push_back({ fromNodeIndex, boneNodeIndices, isSpine, attachNodeIndex });
    }
    for (size_t i = 0; i < m_boneNodeChain.size(); ++i) {
        const auto& chain = m_boneNodeChain[i];
        const auto& node = m_bodyNodes[chain.fromNodeIndex];
        const auto& isSpine = chain.isSpine;
        if (isSpine) {
            m_spineChains.push_back(i);
            continue;
        }
        if (dust3d::BoneMark::Neck == node.boneMark) {
            m_neckChains.push_back(i);
        } else if (dust3d::BoneMark::Tail == node.boneMark) {
            m_tailChains.push_back(i);
        } else if (dust3d::BoneMark::Limb == node.boneMark) {
            if (node.origin.x() > 0) {
                m_leftLimbChains.push_back(i);
            } else if (node.origin.x() < 0) {
                m_rightLimbChains.push_back(i);
            }
        }
    }
}

void RigGenerator::calculateSpineDirection(bool* isVertical)
{
    float left = std::numeric_limits<float>::lowest();
    float right = std::numeric_limits<float>::max();
    float top = std::numeric_limits<float>::lowest();
    float bottom = std::numeric_limits<float>::max();
    auto updateBoundingBox = [&](const std::vector<size_t>& chains) {
        for (const auto& it : chains) {
            const auto& node = m_bodyNodes[m_boneNodeChain[it].fromNodeIndex];
            if (node.origin.y() > top)
                top = (float)node.origin.y();
            if (node.origin.y() < bottom)
                bottom = (float)node.origin.y();
            if (node.origin.z() > left)
                left = (float)node.origin.z();
            if (node.origin.z() < right)
                right = (float)node.origin.z();
        }
    };
    updateBoundingBox(m_leftLimbChains);
    updateBoundingBox(m_rightLimbChains);
    auto zLength = left - right;
    auto yLength = top - bottom;
    *isVertical = yLength >= zLength;
}

void RigGenerator::attachLimbsToSpine()
{
    if (m_spineChains.size() != 1 || m_leftLimbChains.size() != m_rightLimbChains.size()) {
        return;
    }

    m_attachLimbsToSpineNodeIndices.resize(m_leftLimbChains.size());
    for (size_t i = 0; i < m_leftLimbChains.size(); ++i) {
        const auto& leftNode = m_bodyNodes[m_boneNodeChain[m_leftLimbChains[i]].attachNodeIndex];
        const auto& rightNode = m_bodyNodes[m_boneNodeChain[m_rightLimbChains[i]].attachNodeIndex];
        auto limbMiddle = (leftNode.origin + rightNode.origin) * 0.5;
        std::vector<std::pair<size_t, float>> distance2WithSpine;
        auto boneNodeChainIndex = m_spineChains[0];
        const auto& nodeIndices = m_boneNodeChain[boneNodeChainIndex].nodeIndices;
        for (const auto& it : nodeIndices) {
            for (const auto& nodeIndex : it) {
                auto diff = m_bodyNodes[nodeIndex].origin - limbMiddle;
                float d2 = (float)diff.lengthSquared();
                distance2WithSpine.push_back({ nodeIndex, d2 });
            }
        }
        if (distance2WithSpine.empty())
            continue;
        auto nodeIndex = std::min_element(distance2WithSpine.begin(), distance2WithSpine.end(),
            [](const std::pair<size_t, float>& first, const std::pair<size_t, float>& second) {
                return first.second < second.second;
            })
                             ->first;
        m_attachLimbsToSpineNodeIndices[i] = nodeIndex;
        m_virtualJoints.insert(nodeIndex);
    }
}

void RigGenerator::buildSkeleton()
{
    if (m_leftLimbChains.size() != m_rightLimbChains.size()) {
        return;
    } else if (m_leftLimbChains.empty()) {
        return;
    }

    if (m_spineChains.empty()) {
        return;
    }

    calculateSpineDirection(&m_isSpineVertical);

    auto sortLimbChains = [&](std::vector<size_t>& chains) {
        std::sort(chains.begin(), chains.end(), [&](const size_t& first, const size_t& second) {
            if (m_isSpineVertical) {
                return m_bodyNodes[m_boneNodeChain[first].fromNodeIndex].origin.y() < m_bodyNodes[m_boneNodeChain[second].fromNodeIndex].origin.y();
            }
            return m_bodyNodes[m_boneNodeChain[first].fromNodeIndex].origin.z() < m_bodyNodes[m_boneNodeChain[second].fromNodeIndex].origin.z();
        });
    };
    sortLimbChains(m_leftLimbChains);
    sortLimbChains(m_rightLimbChains);

    attachLimbsToSpine();
    extractSpineJoints();
    extractBranchJoints();

    if (m_attachLimbsToSpineJointIndices.empty())
        return;

    size_t rootSpineJointIndex = m_attachLimbsToSpineJointIndices[0];
    size_t lastSpineJointIndex = m_spineJoints.size() - 1;

    m_resultBones = new std::vector<dust3d::RiggerBone>;
    m_resultWeights = new std::map<int, dust3d::RiggerVertexWeights>;

    {
        const auto& firstSpineNode = m_bodyNodes[m_spineJoints[rootSpineJointIndex]];
        dust3d::RiggerBone bone;
        bone.headPosition = dust3d::Vector3(0.0, 0.0, 0.0);
        bone.tailPosition = firstSpineNode.origin;
        bone.headRadius = 0;
        bone.tailRadius = firstSpineNode.radius;
        bone.color = colorWhite();
        bone.name = "Body";
        bone.index = (int)m_resultBones->size();
        bone.parent = -1;
        m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
        m_resultBones->push_back(bone);
    }

    auto attachedBoneIndex = [&](size_t spineJointIndex) {
        if (spineJointIndex == rootSpineJointIndex) {
            return m_boneNameToIndexMap[std::string("Body")];
        }
        return m_boneNameToIndexMap[std::string("Spine") + std::to_string(spineJointIndex - rootSpineJointIndex)];
    };

    for (size_t spineJointIndex = rootSpineJointIndex;
         spineJointIndex + 1 < m_spineJoints.size();
         ++spineJointIndex) {
        const auto& currentNode = m_bodyNodes[m_spineJoints[spineJointIndex]];
        const auto& nextNode = m_bodyNodes[m_spineJoints[spineJointIndex + 1]];
        dust3d::RiggerBone bone;
        bone.headPosition = currentNode.origin;
        bone.tailPosition = nextNode.origin;
        bone.headRadius = currentNode.radius;
        bone.tailRadius = nextNode.radius;
        bone.color = 0 == (spineJointIndex - rootSpineJointIndex) % 2 ? colorWhite() : boneMarkToColor(dust3d::BoneMark::Joint);
        bone.name = std::string("Spine") + std::to_string(spineJointIndex + 1 - rootSpineJointIndex);
        bone.role = dust3d::BoneRole::Spine;
        bone.index = (int)m_resultBones->size();
        bone.parent = attachedBoneIndex(spineJointIndex);
        m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
        m_resultBones->push_back(bone);
        (*m_resultBones)[bone.parent].children.push_back(bone.index);
    }

    auto addSpineLinkBone = [&](size_t limbIndex,
                                const std::vector<std::vector<size_t>>& limbJoints,
                                const std::string& chainPrefix) {
        std::string chainName = chainPrefix + std::to_string(limbIndex + 1);
        const auto& spineJointIndex = m_attachLimbsToSpineJointIndices[limbIndex];
        const auto& spineNode = m_bodyNodes[m_spineJoints[spineJointIndex]];
        const auto& limbFirstNode = m_bodyNodes[limbJoints[limbIndex][0]];
        const auto& parentIndex = attachedBoneIndex(spineJointIndex);
        dust3d::RiggerBone bone;
        bone.headPosition = spineNode.origin;
        bone.tailPosition = limbFirstNode.origin;
        bone.headRadius = spineNode.radius;
        bone.tailRadius = limbFirstNode.radius;
        bone.color = colorWhite();
        bone.name = std::string("Virtual_") + (*m_resultBones)[parentIndex].name + std::string("_") + chainName;
        bone.role = dust3d::BoneRole::Virtual;
        bone.index = (int)m_resultBones->size();
        bone.parent = parentIndex;
        m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
        m_resultBones->push_back(bone);
        (*m_resultBones)[parentIndex].children.push_back(bone.index);
    };

    auto addLimbBone = [&](size_t limbIndex,
                           const std::vector<std::vector<size_t>>& limbJoints,
                           const std::string& chainPrefix) {
        const auto& joints = limbJoints[limbIndex];
        std::string chainName = chainPrefix + std::to_string(limbIndex + 1);
        for (size_t limbJointIndex = 0;
             limbJointIndex + 1 < joints.size();
             ++limbJointIndex) {
            const auto& currentNode = m_bodyNodes[joints[limbJointIndex]];
            const auto& nextNode = m_bodyNodes[joints[limbJointIndex + 1]];
            dust3d::RiggerBone bone;
            bone.headPosition = currentNode.origin;
            bone.tailPosition = nextNode.origin;
            bone.headRadius = currentNode.radius;
            bone.tailRadius = nextNode.radius;
            bone.color = 0 == limbJointIndex % 2 ? boneMarkToColor(dust3d::BoneMark::Limb) : boneMarkToColor(dust3d::BoneMark::Joint);
            bone.name = chainName + std::string("_Joint") + std::to_string(limbJointIndex + 1);
            bone.role = dust3d::BoneRole::Limb;
            bone.side = (chainPrefix.rfind("Left", 0) == 0) ? dust3d::BoneSide::Left : dust3d::BoneSide::Right;
            bone.index = (int)m_resultBones->size();
            if (limbJointIndex > 0) {
                auto parentName = chainName + std::string("_Joint") + std::to_string(limbJointIndex);
                bone.parent = m_boneNameToIndexMap[parentName];
            } else {
                const auto& spineJointIndex = m_attachLimbsToSpineJointIndices[limbIndex];
                const auto& parentIndex = attachedBoneIndex(spineJointIndex);
                auto parentName = std::string("Virtual_") + (*m_resultBones)[parentIndex].name + std::string("_") + chainName;
                bone.parent = m_boneNameToIndexMap[parentName];
            }
            m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
            m_resultBones->push_back(bone);
            (*m_resultBones)[bone.parent].children.push_back(bone.index);
        }
    };

    for (size_t limbIndex = 0;
         limbIndex < m_attachLimbsToSpineJointIndices.size();
         ++limbIndex) {
        addSpineLinkBone(limbIndex, m_leftLimbJoints, std::string("LeftLimb"));
        addSpineLinkBone(limbIndex, m_rightLimbJoints, std::string("RightLimb"));
        addLimbBone(limbIndex, m_leftLimbJoints, std::string("LeftLimb"));
        addLimbBone(limbIndex, m_rightLimbJoints, std::string("RightLimb"));
    }

    if (!m_neckJoints.empty()) {
        for (size_t neckJointIndex = 0;
             neckJointIndex + 1 < m_neckJoints.size();
             ++neckJointIndex) {
            const auto& currentNode = m_bodyNodes[m_neckJoints[neckJointIndex]];
            const auto& nextNode = m_bodyNodes[m_neckJoints[neckJointIndex + 1]];
            dust3d::RiggerBone bone;
            bone.headPosition = currentNode.origin;
            bone.tailPosition = nextNode.origin;
            bone.headRadius = currentNode.radius;
            bone.tailRadius = nextNode.radius;
            bone.color = 0 == neckJointIndex % 2 ? boneMarkToColor(dust3d::BoneMark::Neck) : boneMarkToColor(dust3d::BoneMark::Joint);
            bone.name = std::string("Neck_Joint") + std::to_string(neckJointIndex + 1);
            bone.role = dust3d::BoneRole::Neck;
            bone.index = (int)m_resultBones->size();
            if (neckJointIndex > 0) {
                auto parentName = std::string("Neck_Joint") + std::to_string(neckJointIndex);
                bone.parent = m_boneNameToIndexMap[parentName];
            } else {
                auto parentName = std::string("Spine") + std::to_string(lastSpineJointIndex - rootSpineJointIndex);
                bone.parent = m_boneNameToIndexMap[parentName];
            }
            m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
            m_resultBones->push_back(bone);
            (*m_resultBones)[bone.parent].children.push_back(bone.index);
        }
    }

    if (!m_tailJoints.empty()) {
        std::string nearestSpine = "Body";
        for (int spineJointIndex = (int)rootSpineJointIndex;
             spineJointIndex >= 0;
             --spineJointIndex) {
            if (m_spineJoints[spineJointIndex] == m_tailJoints[0])
                break;
            const auto& currentNode = m_bodyNodes[m_spineJoints[spineJointIndex]];
            const auto& nextNode = spineJointIndex > 0 ? m_bodyNodes[m_spineJoints[spineJointIndex - 1]] : m_bodyNodes[m_tailJoints[0]];
            dust3d::RiggerBone bone;
            bone.headPosition = currentNode.origin;
            bone.tailPosition = nextNode.origin;
            bone.headRadius = currentNode.radius;
            bone.tailRadius = nextNode.radius;
            bone.color = 0 == ((int)rootSpineJointIndex - spineJointIndex) % 2 ? boneMarkToColor(dust3d::BoneMark::Joint) : colorWhite();
            bone.name = std::string("Spine0") + std::to_string((int)rootSpineJointIndex - spineJointIndex + 1);
            bone.role = dust3d::BoneRole::SpineReverse;
            bone.index = (int)m_resultBones->size();
            if ((int)rootSpineJointIndex == spineJointIndex) {
                auto parentName = std::string("Body");
                bone.parent = m_boneNameToIndexMap[parentName];
            } else {
                auto parentName = std::string("Spine0") + std::to_string((int)rootSpineJointIndex - spineJointIndex);
                bone.parent = m_boneNameToIndexMap[parentName];
            }
            m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
            m_resultBones->push_back(bone);
            (*m_resultBones)[bone.parent].children.push_back(bone.index);
            nearestSpine = bone.name;
        }

        for (size_t tailJointIndex = 0;
             tailJointIndex + 1 < m_tailJoints.size();
             ++tailJointIndex) {
            const auto& currentNode = m_bodyNodes[m_tailJoints[tailJointIndex]];
            const auto& nextNode = m_bodyNodes[m_tailJoints[tailJointIndex + 1]];
            dust3d::RiggerBone bone;
            bone.headPosition = currentNode.origin;
            bone.tailPosition = nextNode.origin;
            bone.headRadius = currentNode.radius;
            bone.tailRadius = nextNode.radius;
            bone.color = 0 == tailJointIndex % 2 ? boneMarkToColor(dust3d::BoneMark::Tail) : boneMarkToColor(dust3d::BoneMark::Joint);
            bone.name = std::string("Tail_Joint") + std::to_string(tailJointIndex + 1);
            bone.role = dust3d::BoneRole::Tail;
            bone.index = (int)m_resultBones->size();
            if (tailJointIndex > 0) {
                auto parentName = std::string("Tail_Joint") + std::to_string(tailJointIndex);
                bone.parent = m_boneNameToIndexMap[parentName];
            } else {
                auto parentName = nearestSpine;
                bone.parent = m_boneNameToIndexMap[parentName];
            }
            m_boneNameToIndexMap.insert({ bone.name, (int)bone.index });
            m_resultBones->push_back(bone);
            (*m_resultBones)[bone.parent].children.push_back(bone.index);
        }
    }

    m_isSuccessful = true;
}

void RigGenerator::computeSkinWeights()
{
    if (!m_isSuccessful)
        return;

    auto collectNodeIndices = [&](size_t chainIndex,
                                  std::unordered_map<size_t, size_t>* nodeIndexToContainerMap,
                                  size_t containerIndex) {
        const auto& chain = m_boneNodeChain[chainIndex];
        for (const auto& it : chain.nodeIndices) {
            for (const auto& subIt : it)
                nodeIndexToContainerMap->insert({ subIt, containerIndex });
        }
        nodeIndexToContainerMap->insert({ chain.fromNodeIndex, containerIndex });
    };

    const size_t neckIndex = 0;
    const size_t tailIndex = 1;
    const size_t spineIndex = 2;
    const size_t limbStartIndex = 3;

    std::unordered_map<size_t, size_t> nodeIndicesToBranchMap;

    if (!m_neckChains.empty())
        collectNodeIndices(m_neckChains[0], &nodeIndicesToBranchMap, neckIndex);

    if (!m_tailChains.empty())
        collectNodeIndices(m_tailChains[0], &nodeIndicesToBranchMap, tailIndex);

    if (!m_spineChains.empty())
        collectNodeIndices(m_spineChains[0], &nodeIndicesToBranchMap, spineIndex);

    for (size_t i = 0; i < m_leftLimbChains.size(); ++i) {
        collectNodeIndices(m_leftLimbChains[i], &nodeIndicesToBranchMap,
            limbStartIndex + i);
    }

    for (size_t i = 0; i < m_rightLimbChains.size(); ++i) {
        collectNodeIndices(m_rightLimbChains[i], &nodeIndicesToBranchMap,
            limbStartIndex + m_leftLimbChains.size() + i);
    }

    std::vector<std::vector<size_t>> vertexBranches(limbStartIndex + m_leftLimbChains.size() + m_rightLimbChains.size() + 1);

    // Build lookup from nodeId -> bodyNode index
    std::map<dust3d::Uuid, size_t> nodeIdToIndexMap;
    for (size_t i = 0; i < m_bodyNodes.size(); ++i)
        nodeIdToIndexMap[m_bodyNodes[i].nodeId] = i;

    // Use triangleSourceNodes to assign vertices to branches
    const auto* sourceNodes = m_object->triangleSourceNodes();
    if (sourceNodes) {
        // Build a per-vertex branch assignment using triangle source nodes
        // Each vertex gets the branch of the source node from the first triangle that contains it
        std::vector<int> vertexBranchAssignment(m_object->vertices.size(), -1);
        for (size_t triIdx = 0; triIdx < sourceNodes->size() && triIdx < m_object->triangles.size(); ++triIdx) {
            const auto& src = (*sourceNodes)[triIdx];
            // src.second is nodeId
            auto nodeIt = nodeIdToIndexMap.find(src.second);
            size_t branchIdx = spineIndex;
            if (nodeIt != nodeIdToIndexMap.end()) {
                size_t nodeIndex = nodeIt->second;
                auto findBranch = nodeIndicesToBranchMap.find(nodeIndex);
                if (findBranch != nodeIndicesToBranchMap.end()) {
                    branchIdx = findBranch->second;
                }
            }
            const auto& tri = m_object->triangles[triIdx];
            for (size_t vi = 0; vi < tri.size(); ++vi) {
                int vertexIndex = (int)tri[vi];
                if (vertexIndex >= 0 && (size_t)vertexIndex < vertexBranchAssignment.size()) {
                    // Only assign if not yet assigned (first triangle wins)
                    if (vertexBranchAssignment[vertexIndex] < 0) {
                        vertexBranchAssignment[vertexIndex] = (int)branchIdx;
                    }
                }
            }
        }
        // Collect vertices into branches
        for (size_t vertexIndex = 0; vertexIndex < m_object->vertices.size(); ++vertexIndex) {
            int branchIdx = vertexBranchAssignment[vertexIndex];
            if (branchIdx < 0)
                branchIdx = (int)spineIndex;
            if ((size_t)branchIdx < vertexBranches.size())
                vertexBranches[branchIdx].push_back(vertexIndex);
            else
                vertexBranches[spineIndex].push_back(vertexIndex);
        }
    } else {
        // No source nodes: assign everything to spine
        for (size_t vertexIndex = 0; vertexIndex < m_object->vertices.size(); ++vertexIndex) {
            vertexBranches[spineIndex].push_back(vertexIndex);
        }
    }

    auto findNeckBoneIndex = m_boneNameToIndexMap.find(std::string("Neck_Joint1"));
    if (findNeckBoneIndex != m_boneNameToIndexMap.end()) {
        computeBranchSkinWeights(findNeckBoneIndex->second,
            dust3d::BoneRole::Neck, dust3d::BoneSide::None, vertexBranches[neckIndex],
            &vertexBranches[spineIndex]);
    }

    auto findTailBoneIndex = m_boneNameToIndexMap.find(std::string("Tail_Joint1"));
    if (findTailBoneIndex != m_boneNameToIndexMap.end()) {
        computeBranchSkinWeights(findTailBoneIndex->second,
            dust3d::BoneRole::Tail, dust3d::BoneSide::None, vertexBranches[tailIndex],
            &vertexBranches[spineIndex]);
    }

    for (size_t i = 0; i < m_leftLimbChains.size(); ++i) {
        auto namePrefix = std::string("LeftLimb") + std::to_string(i + 1) + std::string("_");
        auto findLimbBoneIndex = m_boneNameToIndexMap.find(namePrefix + "Joint1");
        if (findLimbBoneIndex != m_boneNameToIndexMap.end()) {
            computeBranchSkinWeights(findLimbBoneIndex->second,
                dust3d::BoneRole::Limb, dust3d::BoneSide::Left, vertexBranches[limbStartIndex + i],
                &vertexBranches[spineIndex]);
        }
    }

    for (size_t i = 0; i < m_rightLimbChains.size(); ++i) {
        auto namePrefix = std::string("RightLimb") + std::to_string(i + 1) + std::string("_");
        auto findLimbBoneIndex = m_boneNameToIndexMap.find(namePrefix + "Joint1");
        if (findLimbBoneIndex != m_boneNameToIndexMap.end()) {
            computeBranchSkinWeights(findLimbBoneIndex->second,
                dust3d::BoneRole::Limb, dust3d::BoneSide::Right, vertexBranches[limbStartIndex + m_leftLimbChains.size() + i],
                &vertexBranches[spineIndex]);
        }
    }

    auto findBackSpineBoneIndex = m_boneNameToIndexMap.find(std::string("Spine01"));
    std::vector<size_t> backSpineVertices;
    auto findSpineBoneIndex = m_boneNameToIndexMap.find(std::string("Spine1"));
    if (findSpineBoneIndex != m_boneNameToIndexMap.end()) {
        computeBranchSkinWeights(findSpineBoneIndex->second,
            dust3d::BoneRole::Spine, dust3d::BoneSide::None, vertexBranches[spineIndex],
            findBackSpineBoneIndex != m_boneNameToIndexMap.end() ? &backSpineVertices : nullptr);
    }

    if (findBackSpineBoneIndex != m_boneNameToIndexMap.end()) {
        computeBranchSkinWeights(findBackSpineBoneIndex->second,
            dust3d::BoneRole::SpineReverse, dust3d::BoneSide::None, backSpineVertices);
    }

    for (auto& it : *m_resultWeights)
        it.second.finalizeWeights();
}

void RigGenerator::computeBranchSkinWeights(size_t fromBoneIndex,
    dust3d::BoneRole expectedRole,
    dust3d::BoneSide expectedSide,
    const std::vector<size_t>& vertexIndices,
    std::vector<size_t>* discardedVertexIndices)
{
    std::vector<size_t> remainVertexIndices = vertexIndices;
    size_t currentBoneIndex = fromBoneIndex;
    while (true) {
        const auto& currentBone = (*m_resultBones)[currentBoneIndex];
        std::vector<size_t> newRemainVertexIndices;
        const auto& parentBone = (*m_resultBones)[currentBone.parent >= 0 ? currentBone.parent : (int)currentBoneIndex];
        auto currentDirection = (currentBone.tailPosition - currentBone.headPosition).normalized();
        auto parentDirection = currentBone.parent <= 0 ? currentDirection : (parentBone.tailPosition - parentBone.headPosition).normalized();
        auto cutNormal = ((parentDirection + currentDirection) * kDirectionAverageScale).normalized();
        auto beginGradientLength = (float)parentBone.headRadius * kBeginGradientRadiusScale;
        auto endGradientLength = (float)parentBone.tailRadius * kEndGradientRadiusScale;
        auto parentLength = (float)(parentBone.tailPosition - parentBone.headPosition).length();
        auto previousBoneIndex = (currentBone.role == dust3d::BoneRole::Virtual) ? parentBone.parent : currentBone.parent;
        for (const auto& vertexIndex : remainVertexIndices) {
            const auto& position = m_object->vertices[vertexIndex];
            auto direction = (position - currentBone.headPosition).normalized();
            if (dust3d::Vector3::dotProduct(direction, cutNormal) > 0) {
                float angle = radianBetweenVectors(direction, currentDirection);
                auto projectedLength = (float)(std::cos(angle) * (position - currentBone.headPosition).length());
                if (projectedLength < 0)
                    projectedLength = 0;
                if (projectedLength <= endGradientLength) {
                    auto factor = kJunctionBlendWeight * (1.0f - projectedLength / endGradientLength);
                    (*m_resultWeights)[vertexIndex].addBone(previousBoneIndex, factor);
                }
                newRemainVertexIndices.push_back(vertexIndex);
                continue;
            }
            if (fromBoneIndex == currentBoneIndex) {
                if (nullptr != discardedVertexIndices)
                    discardedVertexIndices->push_back(vertexIndex);
                else
                    (*m_resultWeights)[vertexIndex].addBone((int)currentBoneIndex, 1.0f);
                continue;
            }
            float angle = radianBetweenVectors(direction, -parentDirection);
            auto projectedLength = (float)(std::cos(angle) * (position - currentBone.headPosition).length());
            if (projectedLength < 0)
                projectedLength = 0;
            if (projectedLength <= endGradientLength) {
                (*m_resultWeights)[vertexIndex].addBone(previousBoneIndex, kJunctionBlendWeight + kJunctionBlendWeight * projectedLength / endGradientLength);
                (*m_resultWeights)[vertexIndex].addBone((int)currentBoneIndex, kJunctionBlendWeight * (1.0f - projectedLength / endGradientLength));
                continue;
            }
            if (projectedLength <= parentLength - beginGradientLength) {
                (*m_resultWeights)[vertexIndex].addBone(previousBoneIndex, 1.0f);
                continue;
            }
            if (projectedLength <= parentLength) {
                auto factor = kJunctionBlendWeight + kJunctionBlendWeight * (parentLength - projectedLength) / beginGradientLength;
                (*m_resultWeights)[vertexIndex].addBone(previousBoneIndex, factor);
                continue;
            }
            auto factor = kJunctionBlendWeight * (1.0f - (projectedLength - parentLength) / beginGradientLength);
            (*m_resultWeights)[vertexIndex].addBone(previousBoneIndex, factor);
            continue;
        }
        remainVertexIndices = newRemainVertexIndices;
        if (currentBone.children.empty() || currentBone.role != expectedRole || currentBone.side != expectedSide) {
            for (const auto& vertexIndex : remainVertexIndices) {
                (*m_resultWeights)[vertexIndex].addBone((int)currentBoneIndex, 1.0f);
            }
            break;
        }
        currentBoneIndex = currentBone.children[0];
    }
}

void RigGenerator::extractJointsFromBoneNodeChain(const BoneNodeChain& boneNodeChain,
    std::vector<size_t>* joints)
{
    const size_t& fromNodeIndex = boneNodeChain.fromNodeIndex;
    const std::vector<std::vector<size_t>>& nodeIndices = boneNodeChain.nodeIndices;
    extractJoints(fromNodeIndex, nodeIndices, joints);
}

void RigGenerator::extractJoints(const size_t& fromNodeIndex,
    const std::vector<std::vector<size_t>>& nodeIndices,
    std::vector<size_t>* joints,
    bool checkLastNoneMarkedNode)
{
    if (joints->empty() || (*joints)[joints->size() - 1] != fromNodeIndex) {
        joints->push_back(fromNodeIndex);
    }
    const auto& fromNode = m_bodyNodes[fromNodeIndex];
    std::vector<std::pair<size_t, float>> nodeIndicesAndDistance2Array;
    for (const auto& it : nodeIndices) {
        for (const auto& nodeIndex : it) {
            const auto& node = m_bodyNodes[nodeIndex];
            auto diff = fromNode.origin - node.origin;
            nodeIndicesAndDistance2Array.push_back({
                nodeIndex,
                (float)diff.lengthSquared(),
            });
        }
    }
    if (nodeIndicesAndDistance2Array.empty())
        return;
    std::sort(nodeIndicesAndDistance2Array.begin(), nodeIndicesAndDistance2Array.end(),
        [](const decltype(nodeIndicesAndDistance2Array)::value_type& first,
            const decltype(nodeIndicesAndDistance2Array)::value_type& second) {
            return first.second < second.second;
        });
    std::vector<size_t> jointIndices;
    for (size_t i = 0; i < nodeIndicesAndDistance2Array.size(); ++i) {
        const auto& item = nodeIndicesAndDistance2Array[i];
        const auto& node = m_bodyNodes[item.first];
        if (dust3d::BoneMark::None != node.boneMark || m_virtualJoints.find(item.first) != m_virtualJoints.end()) {
            jointIndices.push_back(i);
            continue;
        }
    }
    bool appendLastJoint = false;
    if (checkLastNoneMarkedNode && !jointIndices.empty() && jointIndices[jointIndices.size() - 1] + 1 != nodeIndicesAndDistance2Array.size()) {
        appendLastJoint = true;
    }
    if (jointIndices.empty() || appendLastJoint) {
        jointIndices.push_back(nodeIndicesAndDistance2Array.size() - 1);
    }
    for (const auto& itemIndex : jointIndices) {
        const auto& item = nodeIndicesAndDistance2Array[itemIndex];
        if (!joints->empty()) {
            if ((*joints)[joints->size() - 1] == item.first)
                continue;
        }
        joints->push_back(item.first);
    }
}

void RigGenerator::extractBranchJoints()
{
    if (!m_neckChains.empty())
        extractJointsFromBoneNodeChain(m_boneNodeChain[m_neckChains[0]], &m_neckJoints);
    if (!m_tailChains.empty())
        extractJointsFromBoneNodeChain(m_boneNodeChain[m_tailChains[0]], &m_tailJoints);
    m_leftLimbJoints.resize(m_leftLimbChains.size());
    for (size_t i = 0; i < m_leftLimbChains.size(); ++i) {
        extractJointsFromBoneNodeChain(m_boneNodeChain[m_leftLimbChains[i]], &m_leftLimbJoints[i]);
    }
    m_rightLimbJoints.resize(m_rightLimbChains.size());
    for (size_t i = 0; i < m_rightLimbChains.size(); ++i) {
        extractJointsFromBoneNodeChain(m_boneNodeChain[m_rightLimbChains[i]], &m_rightLimbJoints[i]);
    }
}

void RigGenerator::extractSpineJoints()
{
    auto findTail = m_branchNodesMapByMark.find((int)dust3d::BoneMark::Tail);
    if (findTail != m_branchNodesMapByMark.end()) {
        m_spineJoints.push_back(findTail->second[0]);
    }

    if (!m_attachLimbsToSpineNodeIndices.empty() && !m_spineChains.empty()) {
        const auto& fromNodeIndex = findTail == m_branchNodesMapByMark.end() ? m_attachLimbsToSpineNodeIndices[0] : findTail->second[0];
        std::vector<size_t> joints;
        extractJoints(fromNodeIndex,
            m_boneNodeChain[m_spineChains[0]].nodeIndices,
            &joints,
            false);
        m_attachLimbsToSpineJointIndices.resize(m_attachLimbsToSpineNodeIndices.size());
        for (const auto& nodeIndex : joints) {
            for (size_t i = 0; i < m_attachLimbsToSpineNodeIndices.size(); ++i) {
                if (m_attachLimbsToSpineNodeIndices[i] == nodeIndex) {
                    m_attachLimbsToSpineJointIndices[i] = m_spineJoints.size();
                }
            }
            m_spineJoints.push_back(nodeIndex);
        }
    }

    auto findNeck = m_branchNodesMapByMark.find((int)dust3d::BoneMark::Neck);
    if (findNeck != m_branchNodesMapByMark.end()) {
        m_spineJoints.push_back(findNeck->second[0]);
    }
}

void RigGenerator::splitByNodeIndex(size_t nodeIndex,
    std::unordered_set<size_t>* left,
    std::unordered_set<size_t>* right)
{
    const auto& neighbors = m_neighborMap[nodeIndex];
    if (2 != neighbors.size()) {
        return;
    }
    {
        std::unordered_set<size_t> visited;
        visited.insert(*neighbors.begin());
        collectNodes(nodeIndex, left, &visited);
        left->erase(nodeIndex);
    }
    {
        std::unordered_set<size_t> visited;
        visited.insert(*(++neighbors.begin()));
        collectNodes(nodeIndex, right, &visited);
        right->erase(nodeIndex);
    }
}

void RigGenerator::collectNodes(size_t fromNodeIndex,
    std::unordered_set<size_t>* container,
    std::unordered_set<size_t>* visited)
{
    std::queue<size_t> waitQueue;
    waitQueue.push(fromNodeIndex);
    while (!waitQueue.empty()) {
        auto nodeIndex = waitQueue.front();
        waitQueue.pop();
        if (visited->find(nodeIndex) != visited->end())
            continue;
        visited->insert(nodeIndex);
        container->insert(nodeIndex);
        for (const auto& neighborNodeIndex : m_neighborMap[nodeIndex]) {
            if (visited->find(neighborNodeIndex) != visited->end())
                continue;
            waitQueue.push(neighborNodeIndex);
        }
    }
}

void RigGenerator::collectNodesForBoneRecursively(size_t fromNodeIndex,
    const std::unordered_set<size_t>* limitedNodeIndices,
    std::vector<std::vector<size_t>>* boneNodeIndices,
    size_t depth,
    std::unordered_set<size_t>* visited)
{
    std::vector<size_t> nodeIndices;
    for (const auto& nodeIndex : m_neighborMap[fromNodeIndex]) {
        if (limitedNodeIndices->find(nodeIndex) == limitedNodeIndices->end())
            continue;
        if (visited->find(nodeIndex) != visited->end())
            continue;
        visited->insert(nodeIndex);
        if (depth >= boneNodeIndices->size())
            boneNodeIndices->resize(depth + 1);
        (*boneNodeIndices)[depth].push_back(nodeIndex);
        nodeIndices.push_back(nodeIndex);
    }

    for (const auto& nodeIndex : nodeIndices) {
        collectNodesForBoneRecursively(nodeIndex,
            limitedNodeIndices,
            boneNodeIndices,
            depth + 1,
            visited);
    }
}

void RigGenerator::removeBranchsFromNodes(const std::vector<std::vector<size_t>>* boneNodeIndices,
    std::vector<size_t>* resultNodes)
{
    std::unordered_set<size_t> branchSet;
    for (const auto& level : *boneNodeIndices) {
        for (const auto& nodeIndex : level) {
            branchSet.insert(nodeIndex);
        }
    }
    std::vector<size_t> filtered;
    filtered.reserve(resultNodes->size());
    for (const auto& nodeIndex : *resultNodes) {
        if (branchSet.find(nodeIndex) == branchSet.end())
            filtered.push_back(nodeIndex);
    }
    *resultNodes = std::move(filtered);
}

void RigGenerator::generate()
{
    buildNeighborMap();
    buildBoneNodeChain();
    buildSkeleton();
    computeSkinWeights();
}

void RigGenerator::process()
{
    generate();
    this->moveToThread(QGuiApplication::instance()->thread());
    emit finished();
}
