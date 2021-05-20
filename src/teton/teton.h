#ifndef TETON_H
#define TETON_H
#include <string>

#include "../slicer.h"
#include "../sliceDataStorage.h"
#include "Teton.pb.h"

namespace teton {

class Exception {
public:
    Exception(std::string message);

    std::string message_;
};

void sliceDataStorageToTetonMeshes(const cura::SliceDataStorage& storage, std::shared_ptr<teton::proto::Meshes> meshes);
void slicerToTetonMeshOutline(const cura::Slicer& slicer, std::shared_ptr<teton::proto::MeshOutline> outline);
void overrideSlicerOutlines(cura::Slicer& slicer, const teton::proto::MeshOutline& outline);
void writeMeshesToJson(const proto::Meshes& meshes, std::string out_name);

} // namespace teton

#endif // TETON_H

