#ifndef CASHEPH_H
#define CASHEPH_H

#include <time.h>

#include "zlib.h"

typedef struct casheph_s casheph_t;

typedef struct casheph_account_s casheph_account_t;

typedef struct casheph_transaction_s casheph_transaction_t;

typedef struct casheph_split_s casheph_split_t;

struct casheph_s
{
  casheph_account_t *root;
  int n_transactions;
  casheph_transaction_t **transactions;
};

struct casheph_account_s
{
  char *id;
  char *type;
  char *name;
  int n_accounts;
  casheph_account_t **accounts;
  char *parent;
};

struct casheph_transaction_s
{
  char *id;
  time_t date_posted;
  time_t date_entered;
  char *desc;
  int n_splits;
  casheph_split_t *splits;
};

struct casheph_split_s
{
  char *id;
  char *reconciled_state;
  long value;
  char *account;
};

casheph_t *casheph_open (const char *filename);

typedef struct casheph_attribute_s casheph_attribute_t;

struct casheph_attribute_s
{
  char *key;
  char *val;
};

typedef struct casheph_tag_s casheph_tag_t;

struct casheph_tag_s
{
  char *name;
  size_t n_attributes;
  casheph_attribute_t **attributes;
};

casheph_tag_t *casheph_parse_tag (gzFile file);

casheph_attribute_t *casheph_parse_attribute (gzFile file);

void casheph_tag_destroy (casheph_tag_t *tag);

casheph_account_t *casheph_account_get_account_by_name (casheph_account_t *act,
                                                        const char *name);

casheph_transaction_t *casheph_get_transaction (casheph_t *ce,
                                                const char *id);

#endif
