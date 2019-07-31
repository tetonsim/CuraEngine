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

proto::Meshes sliceDataStorageToTetonMeshes(const cura::SliceDataStorage& storage);

} // namespace teton

#endif // TETON_H

