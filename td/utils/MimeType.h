#include "td/utils/Slice.h"
namespace td {
string mime_type_to_extension(Slice mime_type, Slice default_value);
string extension_to_mime_type(Slice extension, Slice default_value = Slice());
}
