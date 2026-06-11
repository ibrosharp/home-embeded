#ifndef JSON_SERIALIZABLE_H
#define JSON_SERIALIZABLE_H

class JsonSerializable {
public:
    virtual ~JsonSerializable() {}
    virtual String toJson() = 0;
};

#endif
