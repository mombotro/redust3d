# Rigging Integration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port the fixed RC6 rigging system into RC9 (master), enabling animal auto-rigging and FBX skeleton export.

**Architecture:** RC6 rigging code lives at `/home/mom/dust3d-rc6/src/`. RC9 uses `dust3d::` namespace types (Vector3, Uuid, Color, std::string) instead of Qt types (QVector3D, QUuid, QColor, QString). The Qt layer (Document, UI) stays Qt-based. New pure-C++ types go in `dust3d/base/`; ported generator classes go in `application/sources/` (Qt OK there). No TBB available in RC9 — replace any `tbb::parallel_for` with sequential loops.

**Tech Stack:** Qt5 C++, qmake (`application/application.pro`), dust3d:: namespace types, no TBB

---

## Reference files

| Path | Purpose |
|------|---------|
| `/home/mom/dust3d-rc6/src/bonemark.h` | Source BoneMark enum + macros |
| `/home/mom/dust3d-rc6/src/rigtype.h` | Source RigType enum + macros |
| `/home/mom/dust3d-rc6/src/rigger.h` | Source BoneRole, BoneSide, RiggerBone, RiggerVertexWeights |
| `/home/mom/dust3d-rc6/src/riggenerator.h` | Source RigGenerator class declaration |
| `/home/mom/dust3d-rc6/src/riggenerator.cpp` | Source RigGenerator implementation |
| `/home/mom/dust3d-rc6/src/animalposer.h` | Source AnimalPoser declaration |
| `/home/mom/dust3d-rc6/src/animalposer.cpp` | Source AnimalPoser implementation |
| `/home/mom/dust3d-rc6/src/poser.h` | Source Poser base class |
| `/home/mom/dust3d-rc6/src/poser.cpp` | Source Poser implementation |
| `/home/mom/dust3d-rc6/src/jointnodetree.h` | Source JointNodeTree |
| `/home/mom/dust3d-rc6/src/jointnodetree.cpp` | Source JointNodeTree implementation |
| `/home/mom/dust3d/dust3d/base/combine_mode.h` | Pattern: how enum headers look |
| `/home/mom/dust3d/dust3d/base/combine_mode.cc` | Pattern: how enum .cc looks |
| `/home/mom/dust3d/dust3d/base/object.h` | ObjectNode and Object class |
| `/home/mom/dust3d/dust3d/mesh/mesh_generator.cc` | Where nodeMap/triangles/vertices are built |
| `/home/mom/dust3d/application/sources/document.h` | Document::Node, Document signals |
| `/home/mom/dust3d/application/sources/document.cc` | toSnapshot/fromSnapshot patterns |
| `/home/mom/dust3d/application/sources/skeleton_graphics_widget.cc` | showContextMenu pattern |
| `/home/mom/dust3d/application/sources/fbx_file.cc` | FBX scaffolding to fill in |
| `/home/mom/dust3d/application/application.pro` | Build file — add all new files here |

## Type translation table

| RC6 type | RC9 type |
|----------|----------|
| `QVector3D` | `dust3d::Vector3` |
| `QUuid` | `dust3d::Uuid` |
| `QColor` | `dust3d::Color` |
| `QString` | `std::string` |
| `QObject::tr(...)` | Quoted string literal (no i18n in base/) |
| `tbb::parallel_for(...)` | Sequential for loop |
| `Outcome*` | `const dust3d::Object*` |
| `m_outcome->bodyNodes[i].origin` | `m_bodyNodes[i].origin` (see Task 6) |
| `m_outcome->bodyNodes[i].boneMark` | `m_bodyNodes[i].boneMark` |
| `m_outcome->triangleSourceNodes()` | `m_object->triangleSourceNodes()` |
| `m_outcome->vertices[i]` | `m_object->vertices[i]` |
| `m_outcome->triangles[i]` | `m_object->triangles[i]` |

---

### Task 1: BoneMark enum

**Files:**
- Create: `dust3d/base/bone_mark.h`
- Create: `dust3d/base/bone_mark.cc`
- Modify: `application/application.pro` — add the two new files

**Reference:** `dust3d/base/combine_mode.h` and `combine_mode.cc` for the exact pattern to follow.
The enum values to port from `/home/mom/dust3d-rc6/src/bonemark.h`: None, Neck, Limb, Tail, Joint, Count.
Also port the helper macros as inline constexpr functions:
- `BoneMarkHasSide(mark)` → `boneMarkHasSide(BoneMark mark)` returning `mark == BoneMark::Limb`
- `BoneMarkIsBranchNode(mark)` → `boneMarkIsBranchNode(BoneMark mark)` returning Neck||Limb||Tail

**Step 1: Create `dust3d/base/bone_mark.h`**

```cpp
#ifndef DUST3D_BASE_BONE_MARK_H_
#define DUST3D_BASE_BONE_MARK_H_

#include <string>

namespace dust3d {

enum class BoneMark {
    None = 0,
    Neck,
    Limb,
    Tail,
    Joint,
    Count
};

inline bool boneMarkHasSide(BoneMark mark) { return mark == BoneMark::Limb; }
inline bool boneMarkIsBranchNode(BoneMark mark)
{
    return mark == BoneMark::Neck || mark == BoneMark::Limb || mark == BoneMark::Tail;
}

BoneMark BoneMarkFromString(const char* markString);
const char* BoneMarkToString(BoneMark mark);
std::string BoneMarkToDispName(BoneMark mark);

}

#endif
```

**Step 2: Create `dust3d/base/bone_mark.cc`**

```cpp
#include <dust3d/base/bone_mark.h>

namespace dust3d {

BoneMark BoneMarkFromString(const char* markString)
{
    std::string mark = markString;
    if (mark == "Neck")  return BoneMark::Neck;
    if (mark == "Limb")  return BoneMark::Limb;
    if (mark == "Tail")  return BoneMark::Tail;
    if (mark == "Joint") return BoneMark::Joint;
    return BoneMark::None;
}

const char* BoneMarkToString(BoneMark mark)
{
    switch (mark) {
    case BoneMark::Neck:  return "Neck";
    case BoneMark::Limb:  return "Limb";
    case BoneMark::Tail:  return "Tail";
    case BoneMark::Joint: return "Joint";
    default:              return "None";
    }
}

std::string BoneMarkToDispName(BoneMark mark)
{
    switch (mark) {
    case BoneMark::Neck:  return "Neck";
    case BoneMark::Limb:  return "Limb";
    case BoneMark::Tail:  return "Tail";
    case BoneMark::Joint: return "Joint";
    default:              return "None";
    }
}

}
```

**Step 3: Add to `application/application.pro`**

Find the block starting with `HEADERS += ../dust3d/base/axis_aligned_bounding_box.h` and add before it (alphabetical order — "bone" comes before "color"):

```
HEADERS += ../dust3d/base/bone_mark.h
SOURCES += ../dust3d/base/bone_mark.cc
```

**Step 4: Build test**

```bash
cd /home/mom/dust3d/application && qmake && make -j4 2>&1 | grep -E "error:|warning:" | head -20
```

Expected: no errors mentioning bone_mark.

**Step 5: Commit**

```bash
cd /home/mom/dust3d
git add dust3d/base/bone_mark.h dust3d/base/bone_mark.cc application/application.pro
git commit -m "feat: add BoneMark enum to dust3d/base"
```

---

### Task 2: RigType enum

**Files:**
- Create: `dust3d/base/rig_type.h`
- Create: `dust3d/base/rig_type.cc`
- Modify: `application/application.pro`

**Reference:** Same combine_mode.cc pattern. Port from `/home/mom/dust3d-rc6/src/rigtype.h`.
Values: None=0, Animal, Count.

**Step 1: Create `dust3d/base/rig_type.h`**

```cpp
#ifndef DUST3D_BASE_RIG_TYPE_H_
#define DUST3D_BASE_RIG_TYPE_H_

#include <string>

namespace dust3d {

enum class RigType {
    None = 0,
    Animal,
    Count
};

RigType RigTypeFromString(const char* typeString);
const char* RigTypeToString(RigType type);
std::string RigTypeToDispName(RigType type);

}

#endif
```

**Step 2: Create `dust3d/base/rig_type.cc`**

```cpp
#include <dust3d/base/rig_type.h>

namespace dust3d {

RigType RigTypeFromString(const char* typeString)
{
    std::string type = typeString;
    if (type == "Animal") return RigType::Animal;
    return RigType::None;
}

const char* RigTypeToString(RigType type)
{
    switch (type) {
    case RigType::Animal: return "Animal";
    default:              return "None";
    }
}

std::string RigTypeToDispName(RigType type)
{
    switch (type) {
    case RigType::Animal: return "Animal";
    default:              return "None";
    }
}

}
```

**Step 3: Add to `application/application.pro`** (after rig_type entries, "rig" comes after "rectangle"):

```
HEADERS += ../dust3d/base/rig_type.h
SOURCES += ../dust3d/base/rig_type.cc
```

**Step 4: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -10
```

**Step 5: Commit**

```bash
cd /home/mom/dust3d
git add dust3d/base/rig_type.h dust3d/base/rig_type.cc application/application.pro
git commit -m "feat: add RigType enum to dust3d/base"
```

---

### Task 3: RiggerBone header

**Files:**
- Create: `dust3d/base/rig_bone.h` (header-only)
- Modify: `application/application.pro`

**Reference:** `/home/mom/dust3d-rc6/src/rigger.h` for struct fields. Use dust3d:: types throughout. No Qt, no TBB.

**Step 1: Create `dust3d/base/rig_bone.h`**

```cpp
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

const std::string kRootBoneName = "Body";

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
```

**Step 2: Add to `application/application.pro`** (header-only, no .cc):

```
HEADERS += ../dust3d/base/rig_bone.h
```

**Step 3: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -10
```

**Step 4: Commit**

```bash
cd /home/mom/dust3d
git add dust3d/base/rig_bone.h application/application.pro
git commit -m "feat: add RiggerBone/BoneRole/BoneSide/RiggerVertexWeights to dust3d/base"
```

---

### Task 4: Extend ObjectNode and Object

**Files:**
- Modify: `dust3d/base/object.h`

Uncomment `partId`, `nodeId`, `radius` in ObjectNode. Add `boneMark`. Add `bodyEdges` to Object.

**Step 1: Read `dust3d/base/object.h` lines 1–60 to see the current ObjectNode definition**

Current ObjectNode looks like:
```cpp
struct ObjectNode {
    //Uuid partId;
    //Uuid nodeId;
    Vector3 origin;
    //float radius = 0.0;
    Color color;
    float smoothCutoffDegrees = 0.0;
    // ... more commented-out fields
};
```

**Step 2: Edit `dust3d/base/object.h`**

Replace the ObjectNode struct with:
```cpp
struct ObjectNode {
    Uuid partId;
    Uuid nodeId;
    Vector3 origin;
    float radius = 0.0f;
    Color color;
    float smoothCutoffDegrees = 0.0f;
    BoneMark boneMark = BoneMark::None;
    //float metalness = 0.0;
    //float roughness = 1.0;
    //Uuid materialId;
    //Uuid mirrorFromPartId;
    //Uuid mirroredByPartId;
    //Vector3 direction;
    //bool joined = true;
};
```

Also add the include at the top of the file (after existing includes):
```cpp
#include <dust3d/base/bone_mark.h>
```

And add `bodyEdges` to the Object class (after `nodeMap`):
```cpp
std::vector<std::pair<Uuid, Uuid>> bodyEdges;  // skeleton node-to-node adjacency
```

**Step 3: Fix aggregate initialization in mesh_generator.cc**

The existing code at lines 501-502 and 691-692 uses aggregate initialization for ObjectNode:
```cpp
ObjectNode { meshNode.origin, color, smoothCutoffDegrees }
```

This will break since we added fields. Change both sites to named initialization:
```cpp
ObjectNode { /*partId=*/{}, /*nodeId=*/meshNode.sourceId, meshNode.origin,
             /*radius=*/(float)meshNode.radius, color, smoothCutoffDegrees }
```

Actually, use named member assignment instead to avoid fragility:
```cpp
// Old (lines 501-502):
componentCache.nodeMap.emplace(std::make_pair(meshNode.sourceId,
    ObjectNode { meshNode.origin, color, smoothCutoffDegrees }));

// New:
{
    ObjectNode node;
    node.nodeId = meshNode.sourceId;
    node.origin = meshNode.origin;
    node.radius = (float)meshNode.radius;
    node.color = color;
    node.smoothCutoffDegrees = smoothCutoffDegrees;
    componentCache.nodeMap.emplace(meshNode.sourceId, std::move(node));
}
```

Apply the same fix to lines 691-692 (the partCache version).

**Step 4: Build test — this must compile cleanly**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -20
```

Expected: no errors.

**Step 5: Commit**

```bash
cd /home/mom/dust3d
git add dust3d/base/object.h dust3d/mesh/mesh_generator.cc
git commit -m "feat: add boneMark/radius/partId/nodeId to ObjectNode, bodyEdges to Object"
```

---

### Task 5: Populate boneMark, bodyEdges, triangleSourceNodes in mesh_generator

**Files:**
- Modify: `dust3d/mesh/mesh_generator.cc`

The mesh generator reads from the snapshot (which stores node attributes as `map<string, string>`).
After this task, `Object::nodeMap` will have correct `boneMark` and `radius`, `Object::bodyEdges`
will have all skeleton edges, and `Object::triangleSourceNodes()` will return per-triangle source info.

**Step 1: Read `dust3d/mesh/mesh_generator.cc` lines 410–465 (fetchPartOrderedNodes)**

This is where MeshNode is built from snapshot data. We can read boneMark here and store it temporarily in MeshNode — but MeshNode doesn't have a boneMark field. Instead, we'll read it when building nodeMap.

**Step 2: Add boneMark read when building nodeMap**

In `fetchPartOrderedNodes`, the snapshot node data is available. We need to pass it along or read it separately.

Better approach: After `componentCache.nodeMap` and `partCache.nodeMap` are built, walk the snapshot nodes and set boneMark on the matching ObjectNode entries.

Add a new method `void MeshGenerator::applyBoneMarksToNodeMap()` that walks `m_snapshot->nodes` and for each node with a "boneMark" key, looks up the nodeId in `m_object->nodeMap` and sets `boneMark`.

In the generate() method, call this after `m_object->nodeMap = componentCache.nodeMap`.

```cpp
void MeshGenerator::applyBoneMarksToNodeMap()
{
    for (const auto& nodeKv : m_snapshot->nodes) {
        const std::string& nodeIdStr = nodeKv.first;
        const auto& attrs = nodeKv.second;
        auto boneMarkIt = attrs.find("boneMark");
        if (boneMarkIt == attrs.end())
            continue;
        BoneMark mark = BoneMarkFromString(boneMarkIt->second.c_str());
        if (mark == BoneMark::None)
            continue;
        Uuid nodeId(nodeIdStr);
        auto nodeIt = m_object->nodeMap.find(nodeId);
        if (nodeIt == m_object->nodeMap.end())
            continue;
        nodeIt->second.boneMark = mark;
    }
}
```

**Step 3: Populate Object::bodyEdges**

After building nodeMap, walk snapshot edges and add edges between nodes that are both in nodeMap:

```cpp
void MeshGenerator::populateBodyEdges()
{
    for (const auto& edgeKv : m_snapshot->edges) {
        const auto& attrs = edgeKv.second;
        auto fromIt = attrs.find("from");
        auto toIt = attrs.find("to");
        if (fromIt == attrs.end() || toIt == attrs.end())
            continue;
        Uuid fromId(fromIt->second);
        Uuid toId(toIt->second);
        if (m_object->nodeMap.find(fromId) == m_object->nodeMap.end())
            continue;
        if (m_object->nodeMap.find(toId) == m_object->nodeMap.end())
            continue;
        m_object->bodyEdges.push_back({ fromId, toId });
    }
}
```

**Step 4: Populate triangleSourceNodes**

After `m_object->triangles` and `m_object->vertices` and `m_object->positionToNodeIdMap` are set, call:

```cpp
void MeshGenerator::populateTriangleSourceNodes()
{
    std::vector<std::pair<Uuid, Uuid>> sourceNodes;
    sourceNodes.reserve(m_object->triangles.size());
    for (const auto& tri : m_object->triangles) {
        Uuid nodeId;
        if (!tri.empty()) {
            auto it = m_object->positionToNodeIdMap.find(
                PositionKey(m_object->vertices[tri[0]]));
            if (it != m_object->positionToNodeIdMap.end())
                nodeId = it->second;
        }
        sourceNodes.push_back({ nodeId, nodeId });
    }
    m_object->setTriangleSourceNodes(sourceNodes);
}
```

**Step 5: Wire the three new methods into generate()**

In the generate() method, after the line `m_object->nodeMap = componentCache.nodeMap;`, add:
```cpp
applyBoneMarksToNodeMap();
populateBodyEdges();
populateTriangleSourceNodes();
```

**Step 6: Add method declarations to mesh_generator.h**

```cpp
void applyBoneMarksToNodeMap();
void populateBodyEdges();
void populateTriangleSourceNodes();
```

**Step 7: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -20
```

**Step 8: Commit**

```bash
cd /home/mom/dust3d
git add dust3d/mesh/mesh_generator.cc dust3d/mesh/mesh_generator.h
git commit -m "feat: populate boneMark, bodyEdges, triangleSourceNodes in mesh_generator"
```

---

### Task 6: Add boneMark to Document::Node + serialization

**Files:**
- Modify: `application/sources/document.h`
- Modify: `application/sources/document.cc`

**Step 1: Add boneMark field to Document::Node in `document.h`**

After the existing `bool hasCutFaceSettings = false;` line, add:
```cpp
dust3d::BoneMark boneMark = dust3d::BoneMark::None;
```

Also add the include at the top of document.h (with the other dust3d includes):
```cpp
#include <dust3d/base/bone_mark.h>
```

Add a slot for setting boneMark (in the `public slots:` section):
```cpp
void setNodeBoneMark(dust3d::Uuid nodeId, dust3d::BoneMark mark);
```

Add signal for boneMark change (in `signals:`):
```cpp
void nodeBoneMarkChanged(dust3d::Uuid nodeId);
```

**Step 2: Implement setNodeBoneMark in `document.cc`**

Following the pattern of `setNodeCutFace` (search for that function ~line 1583):

```cpp
void Document::setNodeBoneMark(dust3d::Uuid nodeId, dust3d::BoneMark mark)
{
    auto node = nodeMap.find(nodeId);
    if (node == nodeMap.end()) {
        qDebug() << "Node" << nodeId.toString().c_str() << "not found";
        return;
    }
    if (node->second.boneMark == mark)
        return;
    node->second.boneMark = mark;
    auto part = partMap.find(node->second.partId);
    if (part != partMap.end())
        part->second.dirty = true;
    emit nodeBoneMarkChanged(nodeId);
    emit skeletonChanged();
}
```

**Step 3: Serialize boneMark in toSnapshot**

In `toSnapshot()`, find the node serialization block (~line 1780, the section that writes `node["cutRotation"]`). After the cutFace serialization block, add:

```cpp
if (nodeIt.second.boneMark != dust3d::BoneMark::None)
    node["boneMark"] = dust3d::BoneMarkToString(nodeIt.second.boneMark);
```

**Step 4: Deserialize boneMark in fromSnapshot**

In `fromSnapshot()`, find the node deserialization block (~line 1960, near `node.radius = ...`). After the cutFace deserialization block, add:

```cpp
const auto& boneMarkIt = nodeKv.second.find("boneMark");
if (boneMarkIt != nodeKv.second.end())
    node.boneMark = dust3d::BoneMarkFromString(boneMarkIt->second.c_str());
```

**Step 5: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -20
```

**Step 6: Commit**

```bash
cd /home/mom/dust3d
git add application/sources/document.h application/sources/document.cc
git commit -m "feat: add boneMark to Document::Node with snapshot serialization"
```

---

### Task 7: Port RigGenerator

**Files:**
- Create: `application/sources/rig_generator.h`
- Create: `application/sources/rig_generator.cc`
- Modify: `application/application.pro`

**Reference source:** `/home/mom/dust3d-rc6/src/riggenerator.h` and `riggenerator.cpp`.

This is the largest task. Read both RC6 files fully before starting. Key translation rules:
- `Outcome*` → `const dust3d::Object*`
- `QVector3D` → `dust3d::Vector3`; `.x()` → `.x`, `.y()` → `.y`, `.z()` → `.z`
- `QVector3D::length()` → `m.length()` (Vector3 has length())
- `QVector3D::lengthSquared()` → `m.lengthSquared()` (check Vector3 API; if missing, use `m.x*m.x + m.y*m.y + m.z*m.z`)
- `QUuid` → `dust3d::Uuid`; `.toString()` stays `.toString()`
- `QString` → `std::string`; `+` stays `+`; `.startsWith()` → check with `str.rfind("prefix", 0) == 0`
- `QObject::tr(...)` → just `(...)` or remove
- `qRadiansToDegrees(x)` → `(x * 180.0 / M_PI)`
- `acos(x)` → `std::acos(x)` (include `<cmath>`)
- `std::max`, `std::min` from `<algorithm>`
- Remove `#include <QDebug>` and `qDebug()` — replace with `dust3dDebug` from `<dust3d/base/debug.h>`
- `tbb::parallel_for(...)` with `GroupEndpointsStitcher` → sequential loop (see below)
- RC6 uses `Outcome.bodyNodes` (indexed vector) — build equivalent from `Object.nodeMap` at start of generate()

**Internal body-node representation:**

Define a local struct inside rig_generator.cc:
```cpp
struct BodyNode {
    dust3d::Uuid nodeId;
    dust3d::Vector3 origin;
    float radius;
    dust3d::BoneMark boneMark;
};
```

At start of `buildNeighborMap()`, build `m_bodyNodes` from `m_object->nodeMap`:
```cpp
m_bodyNodes.clear();
m_bodyNodeIdToIndex.clear();
for (const auto& kv : m_object->nodeMap) {
    m_bodyNodeIdToIndex[kv.first] = m_bodyNodes.size();
    m_bodyNodes.push_back({ kv.first, kv.second.origin,
                             kv.second.radius, kv.second.boneMark });
}
```

Then build neighbor map from `m_object->bodyEdges`:
```cpp
for (const auto& edge : m_object->bodyEdges) {
    auto fromIt = m_bodyNodeIdToIndex.find(edge.first);
    auto toIt   = m_bodyNodeIdToIndex.find(edge.second);
    if (fromIt == m_bodyNodeIdToIndex.end() || toIt == m_bodyNodeIdToIndex.end())
        continue;
    m_neighborMap[fromIt->second].insert(toIt->second);
    m_neighborMap[toIt->second].insert(fromIt->second);
}
```

**GroupEndpointsStitcher (TBB removal):**

RC6 code:
```cpp
tbb::parallel_for(tbb::blocked_range<size_t>(0, groupEndpoints.size()),
    GroupEndpointsStitcher(&m_outcome->bodyNodes, &groups, &groupEndpoints, &stitchResult));
```

Replace with a plain loop:
```cpp
for (size_t i = 0; i < groupEndpoints.size(); ++i) {
    const auto& [groupIndex, nodeIndex] = groupEndpoints[i];
    size_t bestMatch = m_bodyNodes.size();
    float bestDist = std::numeric_limits<float>::max();
    for (size_t j = 0; j < groupEndpoints.size(); ++j) {
        if (groupEndpoints[j].first == groupIndex)
            continue;
        float d2 = 0;
        const auto& a = m_bodyNodes[nodeIndex].origin;
        const auto& b = m_bodyNodes[groupEndpoints[j].second].origin;
        float dx = (float)(a.x - b.x), dy = (float)(a.y - b.y), dz = (float)(a.z - b.z);
        d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < bestDist) {
            bestDist = d2;
            bestMatch = groupEndpoints[j].second;
        }
    }
    stitchResult[i] = { bestMatch, bestDist };
}
```

**Step 1: Create `application/sources/rig_generator.h`**

Model it after RC6's `riggenerator.h` but:
- Include `<dust3d/base/bone_mark.h>`, `<dust3d/base/rig_bone.h>`, `<dust3d/base/rig_type.h>`, `<dust3d/base/object.h>`
- Change `Outcome` → `dust3d::Object`
- Change `RiggerBone` etc. to `dust3d::RiggerBone` etc.
- Add `m_bodyNodes` and `m_bodyNodeIdToIndex` members
- Remove all Qt-specific headers except `<QObject>`, `<QThread>`, `<QString>` (std::string), `<QDebug>`

```cpp
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
    struct BodyNode {
        dust3d::Uuid nodeId;
        dust3d::Vector3 origin;
        float radius;
        dust3d::BoneMark boneMark;
    };

    struct BoneNodeChain {
        size_t fromNodeIndex;
        std::vector<std::vector<size_t>> nodeIndices;
        bool isSpine;
        size_t attachNodeIndex;
    };

    dust3d::RigType m_rigType = dust3d::RigType::None;
    const dust3d::Object* m_object = nullptr;
    std::vector<dust3d::RiggerBone>* m_resultBones = nullptr;
    std::map<int, dust3d::RiggerVertexWeights>* m_resultWeights = nullptr;
    bool m_isSuccessful = false;

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
    void extractJoints(size_t fromNodeIndex,
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
```

**Step 2: Create `application/sources/rig_generator.cc`**

Start by reading the full RC6 source at `/home/mom/dust3d-rc6/src/riggenerator.cpp` and port it:

1. Replace all `#include` Qt headers with their dust3d equivalents or `<cmath>`/`<algorithm>` as needed
2. Replace `m_outcome->bodyNodes[i].X` → `m_bodyNodes[i].X` using BodyNode struct
3. Replace `m_outcome->vertices` → `m_object->vertices`
4. Replace `m_outcome->triangles` → `m_object->triangles`
5. Replace `m_outcome->triangleSourceNodes()` → `m_object->triangleSourceNodes()`
6. Replace `QVector3D` operations (see type table)
7. Remove `buildDemoMesh()` (not needed in RC9 — no debug mesh)
8. Remove `takeOutcome()` (Object is not owned by RigGenerator in RC9)
9. Remove `messages()` (optional; can keep as `std::vector<std::pair<int, std::string>>`)
10. Replace TBB parallel_for with sequential loop (see Task 7 description above)
11. At start of `buildNeighborMap()`, populate `m_bodyNodes` and `m_bodyNodeIdToIndex` from `m_object->nodeMap`

Key method where triangleSourceNodes is used (`computeSkinWeights`):
```cpp
const auto* sourceNodes = m_object->triangleSourceNodes();
if (!sourceNodes) return;
for (size_t triIdx = 0; triIdx < sourceNodes->size(); ++triIdx) {
    const auto& src = (*sourceNodes)[triIdx];
    // src.second is the nodeId
    auto nodeIt = m_bodyNodeIdToIndex.find(src.second);
    if (nodeIt == m_bodyNodeIdToIndex.end()) continue;
    size_t nodeIndex = nodeIt->second;
    // ... rest of weight assignment
}
```

For the SpatialGrid struct (added in RC6 fixes), keep it exactly as-is in rig_generator.cc.

**Step 3: Add to `application/application.pro`**

```
HEADERS += sources/rig_generator.h
SOURCES += sources/rig_generator.cc
```

**Step 4: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -30
```

Fix all compilation errors iteratively before moving on.

**Step 5: Commit**

```bash
cd /home/mom/dust3d
git add application/sources/rig_generator.h application/sources/rig_generator.cc application/application.pro
git commit -m "feat: port RigGenerator from RC6 to RC9 types"
```

---

### Task 8: Wire RigGenerator into Document

**Files:**
- Modify: `application/sources/document.h`
- Modify: `application/sources/document.cc`

**Step 1: Add rig state to Document in `document.h`**

Add includes:
```cpp
#include <dust3d/base/rig_bone.h>
#include <dust3d/base/rig_type.h>
```

Add to public members:
```cpp
dust3d::RigType rigType = dust3d::RigType::None;
std::vector<dust3d::RiggerBone>* rigBones = nullptr;
std::map<int, dust3d::RiggerVertexWeights>* rigWeights = nullptr;
bool isRigValid = false;
```

Add signal:
```cpp
void rigChanged();
```

Add slot:
```cpp
void setRigType(dust3d::RigType type);
```

Add private helpers:
```cpp
void regenerateRig();
void rigReady();
```

Add private members:
```cpp
QThread* m_rigGeneratorThread = nullptr;
```

**Step 2: Add rigType to snapshot serialization**

In `toSnapshot()`, in the canvas/document section, add:
```cpp
if (rigType != dust3d::RigType::None)
    (*snapshot->canvas.emplace("rigType", dust3d::RigTypeToString(rigType)).first).second = dust3d::RigTypeToString(rigType);
```

Actually use this pattern (matching how other canvas properties are saved):
```cpp
snapshot->canvas["rigType"] = dust3d::RigTypeToString(rigType);
```

In `fromSnapshot()`, find where canvas properties are read and add:
```cpp
{
    auto it = snapshot.canvas.find("rigType");
    if (it != snapshot.canvas.end())
        rigType = dust3d::RigTypeFromString(it->second.c_str());
}
```

**Step 3: Implement setRigType, regenerateRig in `document.cc`**

```cpp
void Document::setRigType(dust3d::RigType type)
{
    if (rigType == type)
        return;
    rigType = type;
    regenerateRig();
    emit rigChanged();
    emit optionsChanged();
}

void Document::regenerateRig()
{
    if (m_rigGeneratorThread) {
        // Let current thread finish; it will call regenerateRig again via rigChanged
        return;
    }
    if (rigType == dust3d::RigType::None || !m_resultObject) {
        delete rigBones;
        rigBones = nullptr;
        delete rigWeights;
        rigWeights = nullptr;
        isRigValid = false;
        return;
    }
    QThread* thread = new QThread;
    RigGenerator* generator = new RigGenerator(rigType, *m_resultObject);
    generator->moveToThread(thread);
    connect(thread, &QThread::started, generator, &RigGenerator::process);
    connect(generator, &RigGenerator::finished, this, &Document::rigReady);
    connect(generator, &RigGenerator::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(thread, &QThread::finished, generator, &RigGenerator::deleteLater);
    m_rigGeneratorThread = thread;
    thread->start();
}

void Document::rigReady()
{
    RigGenerator* generator = qobject_cast<RigGenerator*>(sender());
    m_rigGeneratorThread = nullptr;

    delete rigBones;
    delete rigWeights;
    rigBones = generator->takeResultBones();
    rigWeights = generator->takeResultWeights();
    isRigValid = generator->isSuccessful();

    emit rigChanged();
}
```

**Step 4: Trigger regenerateRig when mesh changes**

In `document.cc`, find the `meshReady()` slot (or the equivalent callback where `m_resultObject` is updated). After the object is stored, call `regenerateRig()`:

```cpp
// Find where m_resultObject is assigned (search for "m_resultObject")
// After that assignment, add:
if (rigType != dust3d::RigType::None)
    regenerateRig();
```

Look for where the mesh generator finishes and the result object is stored — search `m_resultObject` in document.cc.

**Step 5: Add include for rig_generator.h to document.cc**

```cpp
#include "rig_generator.h"
```

**Step 6: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -20
```

**Step 7: Commit**

```bash
cd /home/mom/dust3d
git add application/sources/document.h application/sources/document.cc
git commit -m "feat: wire RigGenerator into Document with rigType serialization"
```

---

### Task 9: Bone mark context menu UI

**Files:**
- Modify: `application/sources/skeleton_graphics_widget.cc`
- Modify: `application/sources/skeleton_graphics_widget.h`

**Step 1: Read `skeleton_graphics_widget.h`** to understand existing signals/slots.

**Step 2: Add signal to `skeleton_graphics_widget.h`**

```cpp
void setNodeBoneMarkRequested(dust3d::Uuid nodeId, dust3d::BoneMark mark);
```

Also add include:
```cpp
#include <dust3d/base/bone_mark.h>
```

**Step 3: Add "Mark" submenu in `showContextMenu()` in `skeleton_graphics_widget.cc`**

After the "Colorize" submenu block (around line 280), add:

```cpp
if (isSingleNodeSelected()) {
    dust3d::Uuid selectedNodeId = *m_rangeSelectionSet.begin()->data(1).value<const dust3d::Uuid*>();
    // Actually use the helper to get the single selected node id:
    QMenu* markSubMenu = m_contextMenu->addMenu(tr("Bone Mark"));

    static const struct { const char* label; dust3d::BoneMark mark; } kMarks[] = {
        { "None",  dust3d::BoneMark::None  },
        { "Neck",  dust3d::BoneMark::Neck  },
        { "Limb",  dust3d::BoneMark::Limb  },
        { "Tail",  dust3d::BoneMark::Tail  },
        { "Joint", dust3d::BoneMark::Joint },
    };
    for (const auto& m : kMarks) {
        QAction* act = new QAction(tr(m.label), markSubMenu);
        dust3d::BoneMark bm = m.mark;
        // Need the selected nodeId — get it via singleSelectedNodeId() helper
        connect(act, &QAction::triggered, [=]() {
            emit setNodeBoneMarkRequested(singleSelectedNodeId(), bm);
        });
        markSubMenu->addAction(act);
    }
}
```

**Step 4: Add `singleSelectedNodeId()` helper if not already present**

Add to skeleton_graphics_widget.h:
```cpp
dust3d::Uuid singleSelectedNodeId() const;
```

Implement in skeleton_graphics_widget.cc:
```cpp
dust3d::Uuid SkeletonGraphicsWidget::singleSelectedNodeId() const
{
    for (const auto& it : m_rangeSelectionSet) {
        if (it->data(0) == "node") {
            SkeletonGraphicsNodeItem* nodeItem = static_cast<SkeletonGraphicsNodeItem*>(it);
            return nodeItem->id();
        }
    }
    return dust3d::Uuid();
}
```

**Step 5: Connect signal to Document slot in `document_window.cc`**

Search for where `setNodeCutFace` is connected (it will show the pattern). Add:
```cpp
connect(m_skeletonWidget, &SkeletonGraphicsWidget::setNodeBoneMarkRequested,
        m_document, &Document::setNodeBoneMark);
```

**Step 6: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -20
```

**Step 7: Manual smoke test**

Run the application, right-click a node in the skeleton view, verify "Bone Mark" submenu appears with None/Neck/Limb/Tail/Joint options.

**Step 8: Commit**

```bash
cd /home/mom/dust3d
git add application/sources/skeleton_graphics_widget.h \
        application/sources/skeleton_graphics_widget.cc \
        application/sources/document_window.cc
git commit -m "feat: add bone mark context menu in skeleton view"
```

---

### Task 10: FBX skeleton export

**Files:**
- Modify: `application/sources/fbx_file.h`
- Modify: `application/sources/fbx_file.cc`

**Reference:** RC6 source at `/home/mom/dust3d-rc6/src/fbxfile.cpp` (look for how it exports bones/skin). Also read `/home/mom/dust3d/application/sources/fbx_file.cc` lines 2220–2250 and 3260–3330 where `deformerCount`, `skinId`, `armatureId` are declared but never populated.

**Step 1: Read `fbx_file.h` to understand FbxFileWriter interface**

Find the constructor signature and how `object` and bones/weights are passed.

**Step 2: Extend FbxFileWriter constructor to accept rig data**

Current constructor (search for it in fbx_file.h):
```cpp
FbxFileWriter(const dust3d::Object& object, ...);
```

Add optional rig parameters:
```cpp
FbxFileWriter(const dust3d::Object& object,
              ...,
              const std::vector<dust3d::RiggerBone>* rigBones = nullptr,
              const std::map<int, dust3d::RiggerVertexWeights>* rigWeights = nullptr);
```

Store them as members:
```cpp
const std::vector<dust3d::RiggerBone>* m_rigBones = nullptr;
const std::map<int, dust3d::RiggerVertexWeights>* m_rigWeights = nullptr;
```

**Step 3: Populate deformerCount in fbx_file.cc**

Find `size_t deformerCount = 0;` (line ~2221) and populate it:
```cpp
size_t deformerCount = m_rigBones ? m_rigBones->size() : 0;
```

**Step 4: Generate armature and limb nodes**

Find the block `if (deformerCount > 0)` (~line 2405) and fill in the bone node creation:

For each bone in `*m_rigBones`:
- Create a LimbNode FBX node with the bone's local transform
- Assign a unique int64 ID
- Build parent-child connections

This is the most complex step. Follow the pattern in RC6's fbxfile.cpp exactly, replacing QVector3D with Vector3 and QMatrix4x4 with Matrix4x4.

The key FBX structures needed:
- `armatureId`: root "Armature" null node
- One LimbNode per bone, with `Properties70` containing `Lcl Translation` and `Lcl Rotation`
- One `Deformer` (Skin) connecting mesh to armature
- One `SubDeformer` (Cluster) per bone with `Indexes` and `Weights` arrays
- `BindPose` node listing all bones

**Step 5: Fill in skin weights**

In the SubDeformer block, iterate `m_rigWeights`:
```cpp
for (const auto& kv : *m_rigWeights) {
    int vertexIndex = kv.first;
    for (int i = 0; i < 4; ++i) {
        if (kv.second.boneWeights[i] == 0) break;
        int boneIdx = kv.second.boneIndices[i];
        clusterVertices[boneIdx].push_back(vertexIndex);
        clusterWeights[boneIdx].push_back(kv.second.boneWeights[i]);
    }
}
```

**Step 6: Wire in document_window.cc**

Find where FbxFileWriter is constructed and pass rig data:
```cpp
FbxFileWriter writer(*m_document->m_resultObject, ...,
                     m_document->rigBones,
                     m_document->rigWeights);
```

**Step 7: Build test**

```bash
cd /home/mom/dust3d/application && make -j4 2>&1 | grep -E "error:" | head -20
```

**Step 8: Manual FBX test**

1. Open application, create a simple spine+limb model
2. Mark nodes: one Neck, two Limb pairs, one Tail
3. Set rig type to Animal
4. Export as FBX
5. Open in Blender — verify bones appear attached to mesh with weights

**Step 9: Commit**

```bash
cd /home/mom/dust3d
git add application/sources/fbx_file.h application/sources/fbx_file.cc \
        application/sources/document_window.cc
git commit -m "feat: export skeleton bones and skin weights in FBX"
```

---

## Completion checklist

- [ ] Task 1: `BoneMark` enum compiles cleanly
- [ ] Task 2: `RigType` enum compiles cleanly
- [ ] Task 3: `RiggerBone/RiggerVertexWeights` compiles cleanly
- [ ] Task 4: `ObjectNode` has boneMark/radius/partId/nodeId; `Object` has bodyEdges
- [ ] Task 5: mesh_generator populates boneMark, bodyEdges, triangleSourceNodes
- [ ] Task 6: `Document::Node` has boneMark; snapshot round-trips correctly
- [ ] Task 7: `RigGenerator` compiles and produces bones+weights for a simple test object
- [ ] Task 8: `Document` spawns RigGenerator on mesh change; `rigChanged` signal fires
- [ ] Task 9: Right-click menu shows "Bone Mark" submenu; marks persist after save/load
- [ ] Task 10: FBX export includes armature, limbNodes, skin deformer with weights; imports correctly in Blender
