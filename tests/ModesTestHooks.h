#ifndef SCREENMIRROR_MODE_TEST_HOOKS_H
#define SCREENMIRROR_MODE_TEST_HOOKS_H
/* Host-only scheduling injection; never included in the Blackfin build. */
void screenmirror_test_interleave(unsigned stage);
void screenmirror_test_prepare_cost(void);
#endif
