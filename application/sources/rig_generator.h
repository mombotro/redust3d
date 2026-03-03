#ifndef DUST3D_APPLICATION_RIG_GENERATOR_H_
#define DUST3D_APPLICATION_RIG_GENERATOR_H_

#include <QObject>
#include <QThread>
#include <dust3d/base/bone_mark.h>
#include <dust3d/base/object.h>
#include <dust3d/base/rig_bone.h>
#include <dust3d/base/rig_type.h>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class RigGenerator : public QObject {
    Q_OBJECT
public:
    struct BodyNode {
        dust3d::Uuid nodeId;
        dust3d::Vector3 origin;
        float radius = 0.0f;
        dust3d::BoneMark boneMark = dust3d::BoneMark::None;
    };

    struct BoneNodeChain {
        size_t fromNodeIndex;
        std::vector<std::vector<size_t>> nodeIndices;
        bool isSpine;
        size_t attachNodeIndex;
    };

    RigGenerator(dust3d::RigType rigType, const dust3d::Object& object);
    ~RigGenerator();
    std::vector<dust3d::RiggerBone>* takeResultBones();
    std::map<int, dust3d::RiggerVertexWeights>* takeResultWeights();
    bool isSuccessful() const;
    void generate();

signals:
    void finished();

public slots:
    void process();

private:
    dust3d::RigType m_rigType = dust3d::RigType::None;
    const dust3d::Object* m_object = nullptr;
    std::vector<dust3d::RiggerBone>* m_resultBones = nullptr;
    std::map<int, dust3d::RiggerVertexWeights>* m_resultWeights = nullptr;
    std::vector<BodyNode> m_bodyNodes;
    std::map<dust3d::Uuid, size_t> m_bodyNodeIdToIndex;
    std::map<size_t, std::unordered_set<size_t>> m_neighborMap;
    std::vector<BoneNodeChain> m_boneNodeChain;
    std::vector<size_t> m_neckChains;
    std::vector<size_t> m_leftLimbChains;
    std::vector<size_t> m_rightLimbChains;
    std::vector<size_t> m_tailChains;
    std::vector<size_t> m_spineChains;
    std::unordered_set<size_t> m_virtualJoints;
    std::vector<size_t> m_attachLimbsToSpineNodeIndices;
    std::vector<size_t> m_attachLimbsToSpineJointIndices;
    std::map<int, std::vector<size_t>> m_branchNodesMapByMark;
    std::vector<size_t> m_neckJoints;
    std::vector<std::vector<size_t>> m_leftLimbJoints;
    std::vector<std::vector<size_t>> m_rightLimbJoints;
    std::vector<size_t> m_tailJoints;
    std::vector<size_t> m_spineJoints;
    std::map<std::string, int> m_boneNameToIndexMap;
    bool m_isSpineVertical = false;
    bool m_isSuccessful = false;

    void buildNeighborMap();
    void buildBoneNodeChain();
    void buildSkeleton();
    void computeSkinWeights();
    void calculateSpineDirection(bool* isVertical);
    void attachLimbsToSpine();
    void extractSpineJoints();
    void extractBranchJoints();
    void extractJointsFromBoneNodeChain(const BoneNodeChain& boneNodeChain,
        std::vector<size_t>* joints);
    void extractJoints(const size_t& fromNodeIndex,
        const std::vector<std::vector<size_t>>& nodeIndices,
        std::vector<size_t>* joints,
        bool checkLastNoneMarkedNode = true);
    void groupNodeIndices(const std::map<size_t, std::unordered_set<size_t>>& neighborMap,
        std::vector<std::unordered_set<size_t>>* groups);
    void computeBranchSkinWeights(size_t fromBoneIndex,
        dust3d::BoneRole expectedRole,
        dust3d::BoneSide expectedSide,
        const std::vector<size_t>& vertexIndices,
        std::vector<size_t>* discardedVertexIndices = nullptr);
    void splitByNodeIndex(size_t nodeIndex,
        std::unordered_set<size_t>* left,
        std::unordered_set<size_t>* right);
    void collectNodes(size_t fromNodeIndex,
        std::unordered_set<size_t>* container,
        std::unordered_set<size_t>* visited);
    void collectNodesForBoneRecursively(size_t fromNodeIndex,
        const std::unordered_set<size_t>* limitedNodeIndices,
        std::vector<std::vector<size_t>>* boneNodeIndices,
        size_t depth,
        std::unordered_set<size_t>* visited);
    void removeBranchsFromNodes(const std::vector<std::vector<size_t>>* boneNodeIndices,
        std::vector<size_t>* resultNodes);
};

#endif
