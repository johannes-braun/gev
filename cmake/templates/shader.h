//////////////////////////////////////////////////////////
// THIS FILE IS GENERATED BY CMAKE, PLEASE DO NOT CHANGE!
// Prefix: ${LIBRARY_PREFIX}
// Prefix (uppercase): ${LIBRARY_PREFIX_UPPER}
// Shader: ${SHADER_NAME}
// Shader (C): ${SHADER_NAME_C}
// Namespace: ${SHADER_NAMESPACE}
// Header: ${SHADER_HEADER}
//////////////////////////////////////////////////////////
#ifndef ${LIBRARY_PREFIX_UPPER}_${SHADER_NAME_C}_INCLUDED
#define ${LIBRARY_PREFIX_UPPER}_${SHADER_NAME_C}_INCLUDED

#ifndef SHADER_PACKAGE_USE_C_API
#ifndef __cplusplus
#define SHADER_PACKAGE_USE_C_API
#endif
#endif
#ifndef SHADER_PACKAGE_USE_C_API
#include <cstdint>
#include <span>
#endif
#include <${LIBRARY_PREFIX}/${LIBRARY_PREFIX}_export.h>

#ifndef SHADER_PACKAGE_USE_C_API
namespace ${SHADER_NAMESPACE} {
${LIBRARY_PREFIX_UPPER}_EXPORT extern std::span<std::uint32_t const> ${SHADER_NAME}();
}
#else
#ifdef __cplusplus
extern "C" {
#endif
${LIBRARY_PREFIX_UPPER}_EXPORT extern char const* ${SHADER_NAME_C}();
${LIBRARY_PREFIX_UPPER}_EXPORT extern unsigned long long ${SHADER_NAME_C}_length();
#ifdef __cplusplus
}
#endif
#endif

#endif // ${LIBRARY_PREFIX_UPPER}_${SHADER_NAME_C}_INCLUDED