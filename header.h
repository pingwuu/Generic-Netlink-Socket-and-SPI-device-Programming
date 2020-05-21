
#ifndef GENL_TEST_H
#define GENL_TEST_H

#include <linux/netlink.h>

#ifndef __KERNEL__
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#endif

#define GENLINK_FAMILY_NAME		"genl_test"

#define GENL_TEST_ATTR_MSG_MAX		256

typedef struct _Response Response;

struct _Response
{
  int flagmssg;
  unsigned char patternBuffer[4][8];
  
};

typedef struct _configpins ConfPin;

struct _configpins
{
	int trigg;
	int ech;
	int spics;
};

typedef struct _requestdist Distmsr;

struct _requestdist
{
	char rqmsgs[50];
};

enum {
	GENL_TEST_C_UNSPEC,		/* Must NOT use element 0 */
	GENL_TEST_C_MSG,
	GENL_TEST_C_MSG0,
};

enum genl_test_attrs {
	GENL_TEST_ATTR_UNSPEC,		/* Must NOT use element 0 */

	GENL_TEST_ATTR_MSG,

	GENL_TEST_ATTR_MSG0,

	GENL_TEST_ATTR_MSG1,

	GENL_TEST_ATTR_MSG2,

	__GENL_TEST_ATTR__MAX,
};
#define GENL_TEST_ATTR_MAX (__GENL_TEST_ATTR__MAX - 1)

static struct nla_policy genlink_policy[GENL_TEST_ATTR_MAX+1] = {
	[GENL_TEST_ATTR_MSG] = {
		.type = NLA_NESTED,
	},
	[GENL_TEST_ATTR_MSG0] = {
		.type = NLA_NESTED,
	},
	[GENL_TEST_ATTR_MSG1] = {
		.type = NLA_NESTED,
	},
	[GENL_TEST_ATTR_MSG2] = {
		.type = NLA_U32,
#ifdef __KERNEL__
		.len = GENL_TEST_ATTR_MSG_MAX
#else
		.maxlen = GENL_TEST_ATTR_MSG_MAX
#endif
	}
};

#endif
