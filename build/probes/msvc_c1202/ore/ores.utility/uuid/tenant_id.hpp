// A stand-in for ORE Studio's tenant_id: the entity headers name its type
// and call system(), nothing else.
#ifndef ORES_UTILITY_UUID_TENANT_ID_HPP
#define ORES_UTILITY_UUID_TENANT_ID_HPP
namespace ores::utility::uuid {
class tenant_id final {
public:
    static tenant_id system() { return {}; }
};
}
#endif
