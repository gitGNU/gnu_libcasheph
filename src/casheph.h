/* Copyright (C) 2013 Eric P. Hutchins */

/* This file is part of libcasheph. */

/* libcasheph is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* libcasheph is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with libcasheph.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef CASHEPH_H
#define CASHEPH_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "zlib.h"

typedef struct casheph_s casheph_t;

typedef struct casheph_account_s casheph_account_t;

typedef struct casheph_transaction_s casheph_transaction_t;

typedef struct casheph_split_s casheph_split_t;

typedef struct casheph_slot_s casheph_slot_t;

typedef enum { ce_gdate, ce_frame, ce_guid, ce_string, ce_numeric } casheph_slot_type_t;

typedef struct casheph_gdate_s casheph_gdate_t;

typedef struct casheph_frame_s casheph_frame_t;

typedef struct casheph_commodity_s casheph_commodity_t;

typedef struct casheph_schedxaction_s casheph_schedxaction_t;

typedef struct casheph_schedule_s casheph_schedule_t;

typedef struct casheph_recurrence_s casheph_recurrence_t;

typedef struct casheph_val_s casheph_val_t;

struct casheph_val_s
{
  int32_t n;
  uint32_t d;
};

struct casheph_s
{
  casheph_account_t *root;
  int n_transactions;
  casheph_transaction_t **transactions;
  int n_template_transactions;
  casheph_transaction_t **template_transactions;
  int n_schedxactions;
  casheph_schedxaction_t **schedxactions;
  casheph_account_t *template_root;
  char *book_id;
};

struct casheph_account_s
{
  char *id;
  char *type;
  char *name;
  char *description;
  int n_accounts;
  casheph_account_t **accounts;
  char *parent;
  int n_slots;
  casheph_slot_t **slots;
  casheph_commodity_t *commodity;
  int commodity_scu;
};

struct casheph_transaction_s
{
  char *id;
  time_t date_posted;
  time_t date_entered;
  char *desc;
  int n_splits;
  casheph_split_t **splits;
  int n_slots;
  casheph_slot_t **slots;
};

struct casheph_schedxaction_s
{
  char *id;
  char *name;
  bool enabled;
  bool auto_create;
  bool auto_create_notify;
  int advance_create_days;
  int advance_remind_days;
  int instance_count;
  casheph_gdate_t *start;
  casheph_gdate_t *last;
  char *templ_acct;
  casheph_schedule_t *schedule;
};

struct casheph_split_s
{
  char *id;
  char *reconciled_state;
  casheph_val_t *value;
  casheph_val_t *quantity;
  char *account;
  int n_slots;
  casheph_slot_t **slots;
};

struct casheph_slot_s
{
  char *key;
  casheph_slot_type_t type;
  void *value;
};

struct casheph_frame_s
{
  int n_slots;
  casheph_slot_t **slots;
};

struct casheph_gdate_s
{
  unsigned int year;
  unsigned int month;
  unsigned int day;
};

struct casheph_commodity_s
{
  char *space;
  char *id;
};

struct casheph_schedule_s
{
  int n_recurrences;
  casheph_recurrence_t **recurrences;
};

struct casheph_recurrence_s
{
  int mult;
  char *period_type;
  casheph_gdate_t *start;
  char *weekend_adj;
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

casheph_val_t *casheph_trn_value_for_act (casheph_transaction_t *t, casheph_account_t *act);

void casheph_save (casheph_t *ce, const char *filename);

#endif
