#if defined(SIM_NEED_GIT_COMMIT_ID)
#include ".git-commit-id.h"
#endif
#if !defined(SIM_GIT_COMMIT_ID)
#define FNP_GIT_COMMIT_ID $Format:%H$
#endif

