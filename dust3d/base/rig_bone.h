/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#ifndef DUST3D_BASE_RIG_BONE_H_
#define DUST3D_BASE_RIG_BONE_H_

#include <algorithm>
#include <dust3d/base/color.h>
#include <dust3d/base/uuid.h>
#include <dust3d/base/vector3.h>
#include <string>
#include <utility>
#include <vector>

namespace dust3d {

enum class BoneRole {
    Generic = 0,
    Spine,
    SpineReverse,
    Neck,
    Limb,
    Tail,
    Virtual
};

enum class BoneSide {
    None = 0,
    Left,
    Right
};

constexpr char kRootBoneName[] = "Body";

struct RiggerBone {
    std::string name;
    int index = -1;
    int parent = -1;
    Vector3 headPosition;
    Vector3 tailPosition;
    float headRadius = 0.0f;
    float tailRadius = 0.0f;
    Color color;
    std::vector<int> children;
    BoneRole role = BoneRole::Generic;
    BoneSide side = BoneSide::None;
};

struct RiggerVertexWeights {
    int boneIndices[4] = { 0, 0, 0, 0 };
    float boneWeights[4] = { 0, 0, 0, 0 };

    void addBone(int boneIndex, float weight)
    {
        for (auto& it : m_boneRawWeights) {
            if (it.first == boneIndex) {
                it.second += weight;
                return;
            }
        }
        m_boneRawWeights.push_back({ boneIndex, weight });
    }

    void finalizeWeights()
    {
        std::sort(m_boneRawWeights.begin(), m_boneRawWeights.end(),
            [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                return a.second > b.second;
            });
        float totalWeight = 0;
        for (size_t i = 0; i < m_boneRawWeights.size() && i < 4; i++)
            totalWeight += m_boneRawWeights[i].second;
        if (totalWeight > 0) {
            for (size_t i = 0; i < m_boneRawWeights.size() && i < 4; i++) {
                boneIndices[i] = m_boneRawWeights[i].first;
                boneWeights[i] = m_boneRawWeights[i].second / totalWeight;
            }
        }
    }

private:
    std::vector<std::pair<int, float>> m_boneRawWeights;
};

}

#endif
