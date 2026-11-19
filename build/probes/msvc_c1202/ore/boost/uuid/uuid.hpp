// A stand-in for boost::uuids::uuid: the entity headers only name it.
#ifndef BOOST_UUID_UUID_HPP
#define BOOST_UUID_UUID_HPP
namespace boost::uuids {
struct uuid {
    unsigned char data[16] = {};
};
}
#endif
