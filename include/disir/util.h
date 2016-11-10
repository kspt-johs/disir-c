#ifndef _LIBDISIR_UTIL_H
#define _LIBDISIR_UTIL_H

#ifdef __cplusplus
extern "C"{
#endif // _cplusplus


#include <disir/disir.h>

//! \brief Structure represents a semantic version number.
//!
//! A semantic version number is used to denote the version
//! of any key element within disir is either introduced or deprecrated.
//! An often used abbreviation for semantic version is semver.
//!
struct semantic_version
{
    //! The major number component of a semantic version number.
    uint32_t    sv_major;
    //! The minor number component of a semantic version number.
    uint32_t    sv_minor;
    //! The patch number component of a semantic version number.
    uint32_t    sv_patch;
};

//! \brief Populate the input buffer with a string representation of the semantic version structure
//!
//! If the input buffer is of unsufficient size, NULL is returned
//!
//! \return NULL if buffer or semver are NULL.
//! \return NULL if buffer is of insufficient size.
//! \return buffer when the full string represention was populated into the buffer.
//!
char * dc_semantic_version_string (char *buffer, int32_t buffer_size,
                                   struct semantic_version *semver);

//! \brief Extract a semantic version number from input string and populate the output semver.
//!
//! \param[in] input String where the semantic version number is located
//! \param[out] semver Version structure that is populated with the semantic version
//!     number found in the input string.
//!
//! \return DISIR_STATUS_INVALID_ARGUMENT if input or semver is NULL.
//! \return DISIR_STATUS_INVALID_ARGUMENT if input string does not contain the expected
//!     3 numeric numbers seperated by a period. semver structure is partially updated
//!     with the values it has already detected and parsed.
//! \return DISRI_STATUS_OK on success
//!
enum disir_status dc_semantic_version_convert (const char *input, struct semantic_version *semver);

//! \brief Populate the destination semver with the values of source semver
//!
//! No input validation is performed.
//!
void
dc_semantic_version_set (struct semantic_version *destination, struct semantic_version *source);

//! \brief Compare the two input semantic versions structures
//!
//! \return < 0 if s1 is lesser than s2,
//! \return 0 if s1 == s2
//! \return > 0 if s2 is greater than s1.
//!
int
dc_semantic_version_compare (struct semantic_version *s1, struct semantic_version *s2);


#ifdef __cplusplus
}
#endif // _cplusplus

#endif // _LIBDISIR_UTIL_H
