// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// GL copy targets are intentionally neutral: staging an index store through
// ELEMENT_ARRAY_BUFFER would mutate the accepted VAO. These fixtures observe
// the actual Data call's payload/usage as well as its target. Surface vertex
// blocks allocate static storage without a CPU payload, index blocks upload a
// static payload, and separate scalar blocks upload dynamic payloads.
// Native paint input pools allocate DYNAMIC/null COPY stores; those are
// material resources and intentionally do not count as geometry allocations.
namespace render_upload_test {
inline bool vertex(GLenum target,const void*data,GLenum usage){return target==GL_ARRAY_BUFFER||(target==GL_COPY_WRITE_BUFFER&&((usage==GL_DYNAMIC_DRAW&&data)||(usage==GL_STATIC_DRAW&&!data)));}
inline bool index(GLenum target,const void*data,GLenum usage){return target==GL_ELEMENT_ARRAY_BUFFER||(target==GL_COPY_WRITE_BUFFER&&usage==GL_STATIC_DRAW&&data);}
inline bool subvertex(GLenum target){return target==GL_ARRAY_BUFFER||target==GL_COPY_WRITE_BUFFER;}
inline bool any(GLenum target){return target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER||target==GL_COPY_WRITE_BUFFER;}
}
