#ifndef GVMT_CONFIG_H
#define GVMT_CONFIG_H

#ifdef GVMT_SCHEME_TAGGING

# define GVMT_TAG_MASK 3
# define GVMT_OPAQUE_MASK 1

#else
# ifdef GVMT_ONE_BIT_OPAQUE_TAGGING

#  define GVMT_TAG_MASK 1
#  define GVMT_OPAQUE_MASK 0

# else 

#  define GVMT_TAG_MASK 0
#  define GVMT_OPAQUE_MASK 0

# endif
#endif

#endif // GVMT_CONFIG_H
