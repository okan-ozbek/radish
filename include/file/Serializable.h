//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_SERIALIZABLE_H
#define RADISH_SERIALIZABLE_H

#include <iosfwd>

class Serializable {
public:
    virtual ~Serializable() = default;

    virtual void Serialize(std::ofstream& out) const = 0;
    virtual void Deserialize(std::ifstream& in) = 0;
};

#endif //RADISH_SERIALIZABLE_H