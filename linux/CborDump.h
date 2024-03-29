#ifndef C4459101_F076_4820_A58D_E11FB9A9C0B8
#define C4459101_F076_4820_A58D_E11FB9A9C0B8
#include <cbor.h>
#include <sstream>
#include <Log.h>

CborError dumpCborRecursive(std::stringstream& ss, CborValue *it,
                                          int nestingLevel);
std::string cborDump(const Bytes &);

#endif /* C4459101_F076_4820_A58D_E11FB9A9C0B8 */
