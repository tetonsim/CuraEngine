#ifndef TETON_H
#define TETON_H
#include <string>

#include "../sliceDataStorage.h"
#include "Teton.pb.h"

namespace teton {

class Exception {
public:
    Exception(std::string message);

    std::string message_;
};

void sliceDataStorageToTetonMeshes(const cura::SliceDataStorage& storage, std::shared_ptr<teton::proto::Meshes> meshes);
void writeMeshesToJson(const proto::Meshes& meshes, std::string out_name);

} // namespace teton

#endif // TETON_H

