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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "mxml.h"

#include "casheph.h"

casheph_val_t *
casheph_parse_value_str (const char *str)
{
  casheph_val_t *val = (casheph_val_t*)malloc (sizeof (casheph_val_t));
  int slash_index = -1;
  int i;
  for (i = 0; i < strlen (str); ++i)
    {
      if (str[i] == '/')
        {
          slash_index = i;
          break;
        }
    }
  if (slash_index >= 0)
    {
      int32_t n;
      uint32_t d;
      sscanf (str, "%d/%u", &n, &d);
      val->n = n;
      val->d = d;
    }
  else
    {
      val->d = 1;
      sscanf (str, "%d", &(val->n));
    }
  return val;
}

casheph_account_t *
casheph_account_get_account_by_name (casheph_account_t *act,
                                     const char *name)
{
  int i;
  for (i = 0; i < act->n_accounts; ++i)
    {
      if (strcmp (act->accounts[i]->name, name) == 0)
        {
          return act->accounts[i];
        }
    }
  return NULL;
}

casheph_transaction_t *
casheph_get_transaction (casheph_t *ce,
                         const char *id)
{
  int i;
  for (i = 0; i < ce->n_transactions; ++i)
    {
      if (strcmp (ce->transactions[i]->id, id) == 0)
        {
          return ce->transactions[i];
        }
    }
  return NULL;
}

void
casheph_account_collect_accounts (casheph_account_t *act,
                                  size_t n_accounts,
                                  casheph_account_t **accounts)
{
  int i;
  for (i = 0; i < n_accounts; ++i)
    {
      if (accounts[i]->parent != NULL && strncmp (accounts[i]->parent, act->id, 32) == 0)
        {
          casheph_account_collect_accounts (accounts[i], n_accounts, accounts);
          ++act->n_accounts;
          act->accounts = (casheph_account_t**)realloc (act->accounts,
                                                        sizeof (casheph_account_t*) * act->n_accounts);
          act->accounts[act->n_accounts - 1] = accounts[i];
        }
    }
}

int
casheph_get_xml_declaration (gzFile file)
{
  char buf[40];
  int res = gzread (file, buf, 40);
  if (res != 40)
    {
      return -1;
    }
  if (strncmp (buf, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n", 40) != 0)
    {
      return -1;
    }
  return 0;
}

char *
mxml_load_text (mxml_node_t *txt_node)
{
  char *str = NULL;
  mxml_node_t *txt_val_node = mxmlGetFirstChild (txt_node);
  if (txt_val_node == NULL)
    {
      str = malloc (1);
      str[0] = '\0';
      return str;
    }
  str = strdup (mxmlGetText (txt_val_node, NULL));
  while ((txt_val_node = mxmlGetNextSibling (txt_val_node)) != NULL)
    {
      str = (char*)realloc (str,
                            strlen (str) + strlen (mxmlGetText (txt_val_node,
                                                                NULL)) + 2);
      strcat (str, " ");
      strcat (str, mxmlGetText (txt_val_node, NULL));
    }
  return str;
}

char *
mxml_load_child_text (mxml_node_t *node, const char *name)
{
  char *str = NULL;
  mxml_node_t *ch = mxmlFindElement (node, node, name, NULL, NULL, MXML_DESCEND);
  if (ch)
    {
      str = mxml_load_text (ch);
    }
  return str;
}

bool
mxml_load_child_yn (mxml_node_t *node, const char *name)
{
  bool res = false;
  char *buf = mxml_load_child_text (node, name);
  if (buf != NULL)
    {
      if (buf[0] == 'y')
        {
          res = true;
        }
    }
  free (buf);
  return res;
}

casheph_gdate_t *
mxml_load_gdate (mxml_node_t *slot_val_node)
{
  casheph_gdate_t *date = (casheph_gdate_t*)malloc (sizeof (casheph_gdate_t));
  char *buf = mxml_load_child_text (slot_val_node, "gdate");
  date->year = (buf[0] - '0') * 1000 + (buf[1] - '0') * 1000
    + (buf[2] - '0') * 10 + (buf[3] - '0');
  date->month = (buf[5] - '0') * 10 + (buf[6] - '0');
  date->day = (buf[8] - '0') * 10 + (buf[9] - '0');
  free (buf);
  return date;
}

int
mxml_load_child_int (mxml_node_t *node, const char *name)
{
  int res = -1;
  char *buf = mxml_load_child_text (node, name);
  if (buf)
    {
      sscanf (buf, "%d", &res);
    }
  free (buf);
  return res;
}

casheph_gdate_t *
mxml_load_child_gdate (mxml_node_t *node, const char *name)
{
  mxml_node_t *ch = mxmlFindElement (node, node, name, NULL, NULL, MXML_DESCEND);
  if (ch == NULL)
    {
      return 0;
    }
  return mxml_load_gdate (ch);
}

time_t
mxml_load_child_ts_date (mxml_node_t *node, const char *name)
{
  mxml_node_t *ch = mxmlFindElement (node, node, name, NULL, NULL, MXML_DESCEND);
  if (ch == NULL)
    {
      return 0;
    }
  char *date_str = mxml_load_child_text (ch, "ts:date");

  tzset ();
  struct tm tm;
  memset (&tm, 0, sizeof (struct tm));
  strptime (date_str, "%Y-%m-%d %H:%M:%S", &tm);
  time_t t = mktime (&tm);
  t += tm.tm_gmtoff - (tm.tm_isdst * 3600);
  int minutes_to_add = date_str[24] - '0'
    + (date_str[23] - '0') * 10
    + (date_str[22] - '0') * 60
    + (date_str[21] - '0') * 600;
  int seconds_to_add = minutes_to_add * 60;
  if (date_str[20] == '+')
    {
      seconds_to_add *= -1;
    }
  t += seconds_to_add;

  return t;
}

casheph_val_t *
mxml_load_child_val (mxml_node_t *node, const char *name)
{
  char *buf = mxml_load_child_text (node, name);
  casheph_val_t *val = casheph_parse_value_str (buf);
  free (buf);
  return val;
}

casheph_slot_t *
mxml_load_slot (mxml_node_t *slot_node)
{
  casheph_slot_t *slot = (casheph_slot_t*)malloc (sizeof (casheph_slot_t));
  slot->key = mxml_load_child_text (slot_node, "slot:key");
  mxml_node_t *value = mxmlFindElement (slot_node, slot_node, "slot:value", NULL, NULL, MXML_DESCEND);
  if (strcmp (mxmlElementGetAttr (value, "type"), "gdate") == 0)
    {
      slot->type = ce_gdate;
      slot->value = mxml_load_gdate (value);
    }
  else if (strcmp (mxmlElementGetAttr (value, "type"), "string") == 0)
    {
      slot->type = ce_string;
      slot->value = mxml_load_text (value);
    }
  else if (strcmp (mxmlElementGetAttr (value, "type"), "guid") == 0)
    {
      slot->type = ce_guid;
      slot->value = mxml_load_text (value);
    }
  else if (strcmp (mxmlElementGetAttr (value, "type"), "frame") == 0)
    {
      casheph_frame_t *frame = (casheph_frame_t*)malloc (sizeof (casheph_frame_t));
      frame->n_slots = 0;
      frame->slots = NULL;
      mxml_node_t *in_slot_node = slot_node;
      while ((in_slot_node = mxmlFindElement (in_slot_node, slot_node, "slot", NULL, NULL, MXML_DESCEND)) != NULL)
        {
          casheph_slot_t *in_slot = mxml_load_slot (in_slot_node);
          ++frame->n_slots;
          frame->slots = (casheph_slot_t**)realloc (frame->slots, sizeof (casheph_slot_t*) * frame->n_slots);
          frame->slots[frame->n_slots - 1] = in_slot;
        }
      slot->type = ce_frame;
      slot->value = frame;
    }
  else if (strcmp (mxmlElementGetAttr (value, "type"), "numeric") == 0)
    {
      slot->type = ce_numeric;
      slot->value = casheph_parse_value_str (mxml_load_text (value));
    }
  else
    {
      printf ("slot type: %s\n", mxmlElementGetAttr (value, "type"));
      return NULL;
    }
  return slot;
}

casheph_recurrence_t *
mxml_load_recurrence (mxml_node_t *rec_node)
{
  casheph_recurrence_t *recurrence = (casheph_recurrence_t*)malloc (sizeof (casheph_recurrence_t));
  recurrence->mult = 0;
  recurrence->period_type = NULL;
  recurrence->start = NULL;
  recurrence->weekend_adj = NULL;
  recurrence->mult = mxml_load_child_int (rec_node, "recurrence:mult");
  recurrence->period_type = mxml_load_child_text (rec_node, "recurrence:period_type");
  recurrence->weekend_adj = mxml_load_child_text (rec_node, "recurrence:weekend_adj");
  recurrence->start = mxml_load_child_gdate (rec_node, "recurrence:start");
  return recurrence;
}

casheph_schedule_t *
mxml_load_schedule (mxml_node_t *sched_node)
{
  casheph_schedule_t *schedule = (casheph_schedule_t*)malloc (sizeof (casheph_schedule_t));
  schedule->n_recurrences = 0;
  schedule->recurrences = NULL;
  mxml_node_t *rec_node = sched_node;
  while ((rec_node = mxmlFindElement (rec_node, sched_node, "gnc:recurrence",
                                      NULL, NULL, MXML_DESCEND)) != NULL)
    {
      casheph_recurrence_t *rec = mxml_load_recurrence (rec_node);
      if (rec != NULL)
        {
          ++schedule->n_recurrences;
          schedule->recurrences = (casheph_recurrence_t**)realloc (schedule->recurrences, sizeof (casheph_recurrence_t*) * schedule->n_recurrences);
          schedule->recurrences[schedule->n_recurrences - 1] = rec;
        }
    }
  return schedule;
}

casheph_schedxaction_t *
mxml_load_schedxaction (mxml_node_t *schx_node)
{
  casheph_schedxaction_t *sx = (casheph_schedxaction_t*)malloc (sizeof (casheph_schedxaction_t));
  sx->start = NULL;
  sx->last = NULL;
  sx->schedule = NULL;
  sx->id = mxml_load_child_text (schx_node, "sx:id");
  sx->name = mxml_load_child_text (schx_node, "sx:name");
  sx->enabled = mxml_load_child_yn (schx_node, "sx:enabled");
  sx->auto_create = mxml_load_child_yn (schx_node, "sx:autoCreate");
  sx->auto_create_notify = mxml_load_child_yn (schx_node, "sx:autoCreateNotify");
  sx->advance_create_days = mxml_load_child_int (schx_node, "sx:advanceCreateDays");
  sx->advance_remind_days = mxml_load_child_int (schx_node, "sx:advanceRemindDays");
  sx->instance_count = mxml_load_child_int (schx_node, "sx:instanceCount");
  sx->start = mxml_load_child_gdate (schx_node, "sx:start");
  sx->last = mxml_load_child_gdate (schx_node, "sx:last");
  sx->templ_acct = mxml_load_child_text (schx_node, "sx:templ-acct");
  mxml_node_t *sched_node = mxmlFindElement (schx_node,
                                             schx_node,
                                             "sx:schedule",
                                             NULL, NULL, MXML_DESCEND);
  if (sched_node)
    {
      sx->schedule = mxml_load_schedule (sched_node);
    }
  return sx;
}

casheph_account_t *
mxml_load_account (mxml_node_t *act_node)
{
  casheph_account_t *account = (casheph_account_t*)malloc (sizeof (casheph_account_t));

  account->accounts = NULL;
  account->n_accounts = 0;
  account->slots = NULL;
  account->n_slots = 0;

  account->name = mxml_load_child_text (act_node, "act:name");
  account->type = mxml_load_child_text (act_node, "act:type");
  account->id = mxml_load_child_text (act_node, "act:id");
  account->parent = mxml_load_child_text (act_node, "act:parent");
  account->description = mxml_load_child_text (act_node, "act:description");
  account->commodity = NULL;
  mxml_node_t *cmdty_node = mxmlFindElement (act_node, act_node, "act:commodity", NULL, NULL, MXML_DESCEND);
  if (cmdty_node)
    {
      casheph_commodity_t *commodity = (casheph_commodity_t*)malloc (sizeof (casheph_commodity_t));
      commodity->space = mxml_load_child_text (cmdty_node, "cmdty:space");
      commodity->id = mxml_load_child_text (cmdty_node, "cmdty:id");
      account->commodity = commodity;
    }
  account->commodity_scu = 0;
  char *cmdty_scu = mxml_load_child_text (act_node, "act:commodity-scu");
  if (cmdty_scu != NULL)
    {
      sscanf (cmdty_scu, "%d", &(account->commodity_scu));
    }
  account->n_slots = 0;
  account->slots = NULL;
  mxml_node_t *slots_node = mxmlFindElement (act_node, act_node, "act:slots", NULL, NULL, MXML_DESCEND);
  mxml_node_t *slot_node = slots_node;
  if (slots_node != NULL)
    {
      while ((slot_node = mxmlFindElement (slot_node, slots_node, "slot", NULL, NULL, MXML_DESCEND)) != NULL)
        {
          casheph_slot_t *slot = mxml_load_slot (slot_node);
          ++account->n_slots;
          account->slots = (casheph_slot_t**)realloc (account->slots, sizeof (casheph_slot_t*) * account->n_slots);
          account->slots[account->n_slots - 1] = slot;
        }
    }

  return account;
}

casheph_split_t *
mxml_load_split (mxml_node_t *split_node)
{
  casheph_split_t *split = (casheph_split_t*)malloc (sizeof (casheph_split_t));

  split->id = mxml_load_child_text (split_node, "split:id");
  split->reconciled_state = mxml_load_child_text (split_node, "split:reconciled-state");
  split->account = mxml_load_child_text (split_node, "split:account");
  split->value = mxml_load_child_val (split_node, "split:value");
  split->quantity = mxml_load_child_val (split_node, "split:quantity");
  split->n_slots = 0;
  split->slots = NULL;
  mxml_node_t *slots_node = mxmlFindElement (split_node, split_node, "split:slots", NULL, NULL, MXML_DESCEND);
  mxml_node_t *slot_node = slots_node;
  if (slots_node != NULL)
    {
      while ((slot_node = mxmlFindElement (slot_node, slots_node, "slot", NULL, NULL, MXML_DESCEND_FIRST)) != NULL)
        {
          casheph_slot_t *slot = mxml_load_slot (slot_node);
          ++split->n_slots;
          split->slots = (casheph_slot_t**)realloc (split->slots, sizeof (casheph_slot_t*) * split->n_slots);
          split->slots[split->n_slots - 1] = slot;
        }
    }

  return split;
}

casheph_transaction_t *
mxml_load_transaction (mxml_node_t *trn_node)
{
  casheph_transaction_t *trn = (casheph_transaction_t*)malloc (sizeof (casheph_transaction_t));

  trn->desc = mxml_load_child_text (trn_node, "trn:description");
  trn->id = mxml_load_child_text (trn_node, "trn:id");
  trn->date_posted = mxml_load_child_ts_date (trn_node, "trn:date-posted");
  trn->date_entered = mxml_load_child_ts_date (trn_node, "trn:date-entered");
  mxml_node_t *splits_node = mxmlFindElement (trn_node, trn_node, "trn:splits", NULL, NULL, MXML_DESCEND);
  trn->splits = NULL;
  trn->n_splits = 0;
  mxml_node_t *split_node = splits_node;
  while ((split_node = mxmlFindElement (split_node, splits_node, "trn:split", NULL, NULL, MXML_DESCEND)) != NULL)
    {
      casheph_split_t *split = mxml_load_split (split_node);
      ++trn->n_splits;
      trn->splits = (casheph_split_t**)realloc (trn->splits, sizeof (casheph_split_t*) * trn->n_splits);
      trn->splits[trn->n_splits - 1] = split;
    }
  trn->n_slots = 0;
  trn->slots = NULL;
  mxml_node_t *slots_node = mxmlFindElement (trn_node, trn_node, "trn:slots", NULL, NULL, MXML_DESCEND);
  mxml_node_t *slot_node = slots_node;
  if (slots_node != NULL)
    {
      while ((slot_node = mxmlFindElement (slot_node, slots_node, "slot", NULL, NULL, MXML_DESCEND)) != NULL)
        {
          casheph_slot_t *slot = mxml_load_slot (slot_node);
          ++trn->n_slots;
          trn->slots = (casheph_slot_t**)realloc (trn->slots, sizeof (casheph_slot_t*) * trn->n_slots);
          trn->slots[trn->n_slots - 1] = slot;
        }
    }

  return trn;
}

casheph_t *
casheph_open (const char *filename)
{
  gzFile file2 = gzopen (filename, "r");
  if (file2 == NULL)
    {
      return NULL;
    }
  int file_len = 0;
  char *file_str = NULL;
  char buffer[128];
  int n;
  while ((n = gzread (file2, buffer, 128)) > 0)
    {
      file_len += n;
      file_str = (char*)realloc (file_str, file_len + 1);
      file_str[file_len] = '\0';
      strncpy (file_str + file_len - n, buffer, n);
    }
  gzclose (file2);
  char newb[129];
  newb[128] = '\0';
  strncpy (newb, file_str, 128);

  mxml_node_t *tree = mxmlLoadString (NULL, file_str, MXML_TEXT_CALLBACK);

  if (tree->type != MXML_ELEMENT)
    {
      return NULL;
    }
  if (strncmp (tree->value.element.name, "?xml", 4) != 0)
    {
      return NULL;
    }

  mxml_node_t *gnc_root = mxmlFindElement (tree, tree, "gnc-v2", NULL, NULL, MXML_DESCEND);

  casheph_t *ce = (casheph_t*)malloc (sizeof (casheph_t));
  ce->n_transactions = 0;
  ce->transactions = NULL;
  ce->n_template_transactions = 0;
  ce->template_transactions = NULL;
  ce->template_root = NULL;
  ce->n_schedxactions = 0;
  ce->schedxactions = NULL;
  mxml_node_t *book_id_node = mxmlFindElement (gnc_root, gnc_root, "book:id", NULL, NULL, MXML_DESCEND);
  int whitespace = 0;
  mxml_node_t *book_id_val = mxmlGetFirstChild (book_id_node);

  ce->book_id = strdup (mxmlGetText (book_id_val, NULL));

  mxml_node_t *act_node = NULL;
  casheph_account_t **accounts = NULL;
  int n_accounts = 0;
  act_node = mxmlFindElement (gnc_root,
                              gnc_root,
                              "gnc:account",
                              NULL,
                              NULL,
                              MXML_DESCEND);
  do
    {
      casheph_account_t *account = mxml_load_account (act_node);
      ++n_accounts;
      accounts = (casheph_account_t**)realloc (accounts, sizeof (casheph_account_t*) * n_accounts);
      accounts[n_accounts - 1] = account;
      if (strcmp (account->type, "ROOT") == 0)
        {
          ce->root = account;
        }
    }
  while ((act_node = mxmlFindElement (act_node,
                                      gnc_root,
                                      "gnc:account",
                                      NULL,
                                      NULL,
                                      MXML_NO_DESCEND)) != NULL);

  mxml_node_t *trn_node = NULL;
  trn_node = mxmlFindElement (gnc_root,
                              gnc_root,
                              "gnc:transaction",
                              NULL,
                              NULL,
                              MXML_DESCEND);
  do
    {
      casheph_transaction_t *transaction = mxml_load_transaction (trn_node);
      ++ce->n_transactions;
      ce->transactions = (casheph_transaction_t**)realloc (ce->transactions,
                                                           sizeof (casheph_transaction_t*)
                                                           * ce->n_transactions);
      ce->transactions[ce->n_transactions - 1] = transaction;
    }
  while ((trn_node = mxmlFindElement (trn_node,
                                      gnc_root,
                                      "gnc:transaction",
                                      NULL,
                                      NULL,
                                      MXML_NO_DESCEND)) != NULL);
  mxml_node_t *templ_trns_node = mxmlFindElement (gnc_root, gnc_root, "gnc:template-transactions", NULL, NULL, MXML_DESCEND);
  if (templ_trns_node)
    {
      casheph_account_t **tt_accounts = NULL;
      int n_tt_accounts = 0;
      act_node = templ_trns_node;
      while ((act_node = mxmlFindElement (act_node,
                                          templ_trns_node,
                                          "gnc:account",
                                          NULL,
                                          NULL,
                                          MXML_DESCEND)) != NULL)
        {
          casheph_account_t *account = mxml_load_account (act_node);
          ++n_tt_accounts;
          tt_accounts = (casheph_account_t**)realloc (tt_accounts, sizeof (casheph_account_t*) * n_tt_accounts);
          tt_accounts[n_tt_accounts - 1] = account;
          if (strcmp (account->type, "ROOT") == 0)
            {
              ce->template_root = account;
            }
        }
      trn_node = templ_trns_node;
      while ((trn_node = mxmlFindElement (trn_node,
                                          templ_trns_node,
                                          "gnc:transaction",
                                          NULL,
                                          NULL,
                                          MXML_DESCEND)) != NULL)
        {
          casheph_transaction_t *transaction = mxml_load_transaction (trn_node);
          ++ce->n_template_transactions;
          ce->template_transactions = (casheph_transaction_t**)realloc (ce->template_transactions,
                                                               sizeof (casheph_transaction_t*)
                                                               * ce->n_template_transactions);
          ce->template_transactions[ce->n_template_transactions - 1] = transaction;
        }
      casheph_account_collect_accounts (ce->template_root, n_tt_accounts, tt_accounts);
    }
  mxml_node_t *schx_node = gnc_root;
  while ((schx_node = mxmlFindElement (schx_node,
                                       gnc_root,
                                       "gnc:schedxaction",
                                       NULL,
                                       NULL,
                                       MXML_DESCEND)) != NULL)
    {
      casheph_schedxaction_t *schedxaction = mxml_load_schedxaction (schx_node);
      ++ce->n_schedxactions;
      ce->schedxactions = (casheph_schedxaction_t**)realloc (ce->schedxactions,
                                                             sizeof (casheph_schedxaction_t*)
                                                             * ce->n_schedxactions);
      ce->schedxactions[ce->n_schedxactions - 1] = schedxaction;
    }

  casheph_account_collect_accounts (ce->root, n_accounts, accounts);

  return ce;
}

int
casheph_account_n_sub_accounts (casheph_account_t *account)
{
  int total = 0;
  int i;
  for (i = 0; i < account->n_accounts; ++i)
    {
      total += 1 + casheph_account_n_sub_accounts (account->accounts[i]);
    }
  return total;
}

int
casheph_count_accounts (casheph_t *ce)
{
  return 1 + casheph_account_n_sub_accounts (ce->root);
}

void
casheph_write_commodity (casheph_commodity_t *cmdty, const char *prefix,
                         const char *indent, gzFile file)
{
  gzprintf (file, "%s<%scommodity>\n", indent, prefix);
  gzprintf (file, "%s  <cmdty:space>%s</cmdty:space>\n", indent,
            cmdty->space);
  gzprintf (file, "%s  <cmdty:id>%s</cmdty:id>\n", indent,
            cmdty->id);
  gzprintf (file, "%s</%scommodity>\n", indent, prefix);
}

void
casheph_write_account (casheph_account_t *account, gzFile file)
{
  gzputs (file, "<gnc:account version=\"2.0.0\">\n");
  if (strcmp (account->type, "ROOT") == 0)
    {
      gzprintf (file, "  <act:name>%s</act:name>\n", account->name);
      gzprintf (file, "  <act:id type=\"guid\">%s</act:id>\n", account->id);
      gzprintf (file, "  <act:type>ROOT</act:type>\n");
      if (account->commodity != NULL)
        {
          casheph_write_commodity (account->commodity, "act:", "  ", file);
          gzprintf (file, "  <act:commodity-scu>%d</act:commodity-scu>\n",
                    account->commodity_scu);
        }
    }
  else
    {
      gzprintf (file, "  <act:name>%s</act:name>\n", account->name);
      gzprintf (file, "  <act:id type=\"guid\">%s</act:id>\n", account->id);
      gzprintf (file, "  <act:type>%s</act:type>\n", account->type);
      casheph_write_commodity (account->commodity, "act:", "  ", file);
      gzprintf (file, "  <act:commodity-scu>%d</act:commodity-scu>\n",
                account->commodity_scu);
      if (account->description != NULL)
        {
          gzprintf (file, "  <act:description>%s</act:description>\n", account->description);
        }
      if (account->n_slots > 0)
        {
          gzprintf (file, "  <act:slots>\n");
          int i;
          for (i = 0; i < account->n_slots; ++i)
            {
              gzprintf (file, "    <slot>\n");
              gzprintf (file, "      <slot:key>placeholder</slot:key>\n");
              gzprintf (file, "      <slot:value type=\"string\">true</slot:value>\n");
              gzprintf (file, "    </slot>\n");
            }
          gzprintf (file, "  </act:slots>\n");
        }
      gzprintf (file, "  <act:parent type=\"guid\">%s</act:parent>\n", account->parent);
    }
  gzputs (file, "</gnc:account>\n");
}

void
casheph_write_accounts (casheph_account_t *account, gzFile file)
{
  casheph_write_account (account, file);
  int i;
  for (i = 0; i < account->n_accounts; ++i)
    {
      casheph_write_accounts (account->accounts[i], file);
    }
}

void
casheph_write_slot (const char *indent, casheph_slot_t *slot, gzFile file)
{
  gzprintf (file, "%s<slot>\n", indent);
  gzprintf (file, "%s  <slot:key>%s</slot:key>\n", indent, slot->key);
  casheph_gdate_t *date;
  casheph_frame_t *frame;
  int i;
  char nextindent[128];
  strcpy (nextindent, indent);
  strcat (nextindent, "    ");
  switch (slot->type)
    {
    case ce_gdate:
      date = (casheph_gdate_t*)slot->value;
      gzprintf (file, "%s  <slot:value type=\"gdate\">\n", indent);
      gzprintf (file, "%s    <gdate>%04d-%02d-%02d</gdate>\n", indent,
                date->year, date->month, date->day);
      gzprintf (file, "%s  </slot:value>\n", indent);
      break;
    case ce_numeric:
      gzprintf (file, "%s  <slot:value type=\"numeric\">%d/%d</slot:value>\n",
                indent,
                ((casheph_val_t*)slot->value)->n,
                ((casheph_val_t*)slot->value)->d);
      break;
    case ce_guid:
      gzprintf (file, "%s  <slot:value type=\"guid\">%s</slot:value>\n", indent, (char*)slot->value);
      break;
    case ce_string:
      gzprintf (file, "%s  <slot:value type=\"string\">%s</slot:value>\n", indent, (char*)slot->value);
      break;
    case ce_frame:
      frame = (casheph_frame_t*)slot->value;
      gzprintf (file, "%s  <slot:value type=\"frame\">\n", indent);
      for (i = 0; i < frame->n_slots; ++i)
        {
          casheph_write_slot (nextindent, frame->slots[i], file);
        }
      gzprintf (file, "%s  </slot:value>\n", indent);
      break;
    }
  gzprintf (file, "%s</slot>\n", indent);
}

casheph_slot_t *
casheph_trn_get_slot (casheph_transaction_t *trn, const char *key)
{
  int i;
  for (i = 0; i < trn->n_slots; ++i)
    {
      if (strcmp (trn->slots[i]->key, key) == 0)
        {
          return trn->slots[i];
        }
    }
  return NULL;
}

void
casheph_write_transaction (casheph_transaction_t *trn, gzFile file)
{
  gzputs (file, "<gnc:transaction version=\"2.0.0\">\n");
  gzprintf (file, "  <trn:id type=\"guid\">%s</trn:id>\n", trn->id);
  gzprintf (file, "  <trn:currency>\n");
  gzprintf (file, "    <cmdty:space>ISO4217</cmdty:space>\n");
  gzprintf (file, "    <cmdty:id>USD</cmdty:id>\n");
  gzprintf (file, "  </trn:currency>\n");
  gzprintf (file, "  <trn:date-posted>\n");
  struct tm *tm = localtime (&trn->date_posted);
  int hrs_off = timezone / 3600 - tm->tm_isdst;
  int mins_off = (timezone / 60) % 60;
  char sign = '+';
  if (timezone > 0)
    {
      sign = '-';
    }
  char buf[1024];
  strftime (buf, 1024, "%Y-%m-%d %H:%M:%S", tm);
  gzprintf (file, "    <ts:date>%s %c%02d%02d</ts:date>\n", buf, sign, hrs_off, mins_off);
  gzprintf (file, "  </trn:date-posted>\n");
  gzprintf (file, "  <trn:date-entered>\n");
  tm = localtime (&trn->date_entered);
  hrs_off = timezone / 3600 - tm->tm_isdst;
  mins_off = (timezone / 60) % 60;
  sign = '+';
  if (timezone > 0)
    {
      sign = '-';
    }
  strftime (buf, 1024, "%Y-%m-%d %H:%M:%S", tm);
  gzprintf (file, "    <ts:date>%s %c%02d%02d</ts:date>\n", buf, sign, hrs_off, mins_off);
  gzprintf (file, "  </trn:date-entered>\n");
  gzprintf (file, "  <trn:description>%s</trn:description>\n", trn->desc);
  if (trn->n_slots > 0)
    {
      gzprintf (file, "  <trn:slots>\n");
      int i;
      for (i = 0; i < trn->n_slots; ++i)
        {
          casheph_write_slot ("    ", trn->slots[i], file);
        }
      gzprintf (file, "  </trn:slots>\n");
    }
  if (trn->n_splits > 0)
    {
      gzprintf (file, "  <trn:splits>\n");
      int i;
      for (i = 0; i < trn->n_splits; ++i)
        {
          gzprintf (file, "    <trn:split>\n");
          gzprintf (file, "      <split:id type=\"guid\">%s</split:id>\n", trn->splits[i]->id);
          gzprintf (file, "      <split:reconciled-state>%s</split:reconciled-state>\n", trn->splits[i]->reconciled_state);
          casheph_val_t *val = trn->splits[i]->value;
          int32_t n = val->n;
          uint32_t d = val->d;
          gzprintf (file, "      <split:value>%d/%d</split:value>\n", n, d);
          val = trn->splits[i]->quantity;
          n = val->n;
          d = val->d;
          gzprintf (file, "      <split:quantity>%d/%d</split:quantity>\n", n, d);
          gzprintf (file, "      <split:account type=\"guid\">%s</split:account>\n", trn->splits[i]->account);
          if (trn->splits[i]->n_slots > 0)
            {
              gzprintf (file, "      <split:slots>\n");
              int j;
              for (j = 0; j < trn->splits[i]->n_slots; ++j)
                {
                  casheph_write_slot ("        ", trn->splits[i]->slots[j], file);
                }
              gzprintf (file, "      </split:slots>\n");
            }
          gzprintf (file, "    </trn:split>\n");
        }
      gzprintf (file, "  </trn:splits>\n");
    }
  gzputs (file, "</gnc:transaction>\n");
}

void
casheph_write_recurrence (casheph_recurrence_t *recurrence, gzFile file)
{
  gzputs (file, "    <gnc:recurrence version=\"1.0.0\">\n");
  gzprintf (file, "      <recurrence:mult>%d</recurrence:mult>\n",
            recurrence->mult);
  gzprintf (file, "      <recurrence:period_type>%s</recurrence:period_type>\n",
            recurrence->period_type);
  gzputs (file, "      <recurrence:start>\n");
  gzprintf (file, "        <gdate>%04d-%02d-%02d</gdate>\n",
            recurrence->start->year,
            recurrence->start->month,
            recurrence->start->day);
  gzputs (file, "      </recurrence:start>\n");
  if (recurrence->weekend_adj != NULL)
    {
      gzprintf (file, "      <recurrence:weekend_adj>%s</recurrence:weekend_adj>\n",
                recurrence->weekend_adj);
    }
  gzputs (file, "    </gnc:recurrence>\n");
}

void
casheph_write_schedule (casheph_schedule_t *schedule, gzFile file)
{
  gzputs (file, "  <sx:schedule>\n");
  int i;
  for (i = 0; i < schedule->n_recurrences; ++i)
    {
      casheph_write_recurrence (schedule->recurrences[i], file);
    }
  gzputs (file, "  </sx:schedule>\n");
}

void
casheph_write_schedxaction (casheph_schedxaction_t *sx, gzFile file)
{
  gzputs (file, "<gnc:schedxaction version=\"2.0.0\">\n");
  gzprintf (file, "  <sx:id type=\"guid\">%s</sx:id>\n",
            sx->id);
  gzprintf (file, "  <sx:name>%s</sx:name>\n",
            sx->name);
  gzprintf (file, "  <sx:enabled>%c</sx:enabled>\n",
            (sx->enabled?'y':'n'));
  gzprintf (file, "  <sx:autoCreate>%c</sx:autoCreate>\n",
            (sx->auto_create?'y':'n'));
  gzprintf (file, "  <sx:autoCreateNotify>%c</sx:autoCreateNotify>\n",
            (sx->auto_create_notify?'y':'n'));
  gzprintf (file, "  <sx:advanceCreateDays>%d</sx:advanceCreateDays>\n",
            sx->advance_create_days);
  gzprintf (file, "  <sx:advanceRemindDays>%d</sx:advanceRemindDays>\n",
            sx->advance_remind_days);
  gzprintf (file, "  <sx:instanceCount>%d</sx:instanceCount>\n",
            sx->instance_count);
  if (sx->start != NULL)
    {
      gzputs (file, "  <sx:start>\n");
      gzprintf (file, "    <gdate>%04d-%02d-%02d</gdate>\n",
                sx->start->year,
                sx->start->month,
                sx->start->day);
      gzputs (file, "  </sx:start>\n");
    }
  if (sx->last != NULL)
    {
      gzputs (file, "  <sx:last>\n");
      gzprintf (file, "    <gdate>%04d-%02d-%02d</gdate>\n",
                sx->last->year,
                sx->last->month,
                sx->last->day);
      gzputs (file, "  </sx:last>\n");
    }
  gzprintf (file, "  <sx:templ-acct type=\"guid\">%s</sx:templ-acct>\n",
            sx->templ_acct);
  casheph_write_schedule (sx->schedule, file);
  gzputs (file, "</gnc:schedxaction>\n");
}

void
casheph_write_transactions (casheph_t *ce, gzFile file)
{
  int i;
  for (i = 0; i < ce->n_transactions; ++i)
    {
      casheph_write_transaction (ce->transactions[i], file);
    }
}

void
casheph_write_schedxactions (casheph_t *ce, gzFile file)
{
  int i;
  for (i = 0; i < ce->n_schedxactions; ++i)
    {
      casheph_write_schedxaction (ce->schedxactions[i], file);
    }
}

void
casheph_write_template_transactions (casheph_t *ce, gzFile file)
{
  int i;
  for (i = 0; i < ce->n_template_transactions; ++i)
    {
      casheph_write_transaction (ce->template_transactions[i], file);
    }
}

casheph_val_t *
casheph_trn_value_for_act (casheph_transaction_t *trn, casheph_account_t *act)
{
  casheph_val_t *val = NULL;
  int i;
  for (i = 0; i < trn->n_splits; ++i)
    {
      if (strcmp (trn->splits[i]->account, act->id) == 0)
        {
          val = trn->splits[i]->value;
          break;
        }
    }
  return val;
}

void
casheph_save (casheph_t *ce, const char *filename)
{
  gzFile file = gzopen (filename, "w");
  gzputs (file, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
  gzputs (file, "<gnc-v2\n");
  gzputs (file, "     xmlns:gnc=\"http://www.gnucash.org/XML/gnc\"\n");
  gzputs (file, "     xmlns:act=\"http://www.gnucash.org/XML/act\"\n");
  gzputs (file, "     xmlns:book=\"http://www.gnucash.org/XML/book\"\n");
  gzputs (file, "     xmlns:cd=\"http://www.gnucash.org/XML/cd\"\n");
  gzputs (file, "     xmlns:cmdty=\"http://www.gnucash.org/XML/cmdty\"\n");
  gzputs (file, "     xmlns:price=\"http://www.gnucash.org/XML/price\"\n");
  gzputs (file, "     xmlns:slot=\"http://www.gnucash.org/XML/slot\"\n");
  gzputs (file, "     xmlns:split=\"http://www.gnucash.org/XML/split\"\n");
  gzputs (file, "     xmlns:sx=\"http://www.gnucash.org/XML/sx\"\n");
  gzputs (file, "     xmlns:trn=\"http://www.gnucash.org/XML/trn\"\n");
  gzputs (file, "     xmlns:ts=\"http://www.gnucash.org/XML/ts\"\n");
  gzputs (file, "     xmlns:fs=\"http://www.gnucash.org/XML/fs\"\n");
  gzputs (file, "     xmlns:bgt=\"http://www.gnucash.org/XML/bgt\"\n");
  gzputs (file, "     xmlns:recurrence=\"http://www.gnucash.org/XML/recurrence\"\n");
  gzputs (file, "     xmlns:lot=\"http://www.gnucash.org/XML/lot\"\n");
  gzputs (file, "     xmlns:addr=\"http://www.gnucash.org/XML/addr\"\n");
  gzputs (file, "     xmlns:owner=\"http://www.gnucash.org/XML/owner\"\n");
  gzputs (file, "     xmlns:billterm=\"http://www.gnucash.org/XML/billterm\"\n");
  gzputs (file, "     xmlns:bt-days=\"http://www.gnucash.org/XML/bt-days\"\n");
  gzputs (file, "     xmlns:bt-prox=\"http://www.gnucash.org/XML/bt-prox\"\n");
  gzputs (file, "     xmlns:cust=\"http://www.gnucash.org/XML/cust\"\n");
  gzputs (file, "     xmlns:employee=\"http://www.gnucash.org/XML/employee\"\n");
  gzputs (file, "     xmlns:entry=\"http://www.gnucash.org/XML/entry\"\n");
  gzputs (file, "     xmlns:invoice=\"http://www.gnucash.org/XML/invoice\"\n");
  gzputs (file, "     xmlns:job=\"http://www.gnucash.org/XML/job\"\n");
  gzputs (file, "     xmlns:order=\"http://www.gnucash.org/XML/order\"\n");
  gzputs (file, "     xmlns:taxtable=\"http://www.gnucash.org/XML/taxtable\"\n");
  gzputs (file, "     xmlns:tte=\"http://www.gnucash.org/XML/tte\"\n");
  gzputs (file, "     xmlns:vendor=\"http://www.gnucash.org/XML/vendor\">\n");
  gzputs (file, "<gnc:count-data cd:type=\"book\">1</gnc:count-data>\n");
  gzputs (file, "<gnc:book version=\"2.0.0\">\n");
  gzprintf (file, "<book:id type=\"guid\">%s</book:id>\n", ce->book_id);
  gzputs (file, "<gnc:count-data cd:type=\"commodity\">1</gnc:count-data>\n");
  gzprintf (file, "<gnc:count-data cd:type=\"account\">%d</gnc:count-data>\n",
            casheph_count_accounts (ce));
  gzprintf (file, "<gnc:count-data cd:type=\"transaction\">%d</gnc:count-data>\n",
            ce->n_transactions);
  if (ce->n_template_transactions > 0)
    {
      gzprintf (file, "<gnc:count-data cd:type=\"schedxaction\">%d</gnc:count-data>\n",
                ce->n_template_transactions);
    }
  gzputs (file, "<gnc:commodity version=\"2.0.0\">\n\
  <cmdty:space>ISO4217</cmdty:space>\n\
  <cmdty:id>USD</cmdty:id>\n\
  <cmdty:get_quotes/>\n\
  <cmdty:quote_source>currency</cmdty:quote_source>\n\
  <cmdty:quote_tz/>\n\
</gnc:commodity>\n\
<gnc:commodity version=\"2.0.0\">\n\
  <cmdty:space>template</cmdty:space>\n\
  <cmdty:id>template</cmdty:id>\n\
  <cmdty:name>template</cmdty:name>\n\
  <cmdty:xcode>template</cmdty:xcode>\n\
  <cmdty:fraction>1</cmdty:fraction>\n\
</gnc:commodity>\n");

  casheph_write_accounts (ce->root, file);
  casheph_write_transactions (ce, file);

  if (ce->n_template_transactions > 0)
    {
      gzputs (file, "<gnc:template-transactions>\n");
      casheph_write_accounts (ce->template_root, file);
      casheph_write_template_transactions (ce, file);
      gzputs (file, "</gnc:template-transactions>\n");
      casheph_write_schedxactions (ce, file);
    }

  gzputs (file, "</gnc:book>\n");
  gzputs (file, "</gnc-v2>\n");
  gzputs (file, "\n");
  gzputs (file, "<!-- Local variables: -->\n");
  gzputs (file, "<!-- mode: xml        -->\n");
  gzputs (file, "<!-- End:             -->\n");
  gzclose (file);
}

void
casheph_val_destroy (casheph_val_t *v)
{
  free (v);
}

void casheph_slot_destroy (casheph_slot_t *s);

void
casheph_frame_destroy (casheph_frame_t *f)
{
  int i;
  for (i = 0; i < f->n_slots; ++i)
    {
      casheph_slot_destroy (f->slots[i]);
    }
  free (f->slots);
  free (f);
}

void
casheph_slot_destroy (casheph_slot_t *s)
{
  free (s->key);
  switch (s->type)
    {
    case ce_gdate:
      free (s->value);
      break;
    case ce_string:
      free (s->value);
      break;
    case ce_guid:
      free (s->value);
      break;
    case ce_numeric:
      casheph_val_destroy (s->value);
      break;
    case ce_frame:
      casheph_frame_destroy (s->value);
      break;
    }
  free (s);
}

void
casheph_split_destroy (casheph_split_t *s)
{
  free (s->id);
  free (s->reconciled_state);
  casheph_val_destroy (s->value);
  casheph_val_destroy (s->quantity);
  free (s->account);
  int i;
  for (i = 0; i < s->n_slots; ++i)
    {
      casheph_slot_destroy (s->slots[i]);
    }
  free (s->slots);
  free (s);
}

void
casheph_trn_destroy (casheph_transaction_t *t)
{
  free (t->id);
  free (t->desc);
  int i;
  for (i = 0; i < t->n_splits; ++i)
    {
      casheph_split_destroy (t->splits[i]);
    }
  free (t->splits);
  for (i = 0; i < t->n_slots; ++i)
    {
      casheph_slot_destroy (t->slots[i]);
    }
  free (t->slots);
  free (t);
}

void
casheph_remove_trn (casheph_t *ce, const char *id)
{
  int index = -1;
  int i;
  for (i = 0; i < ce->n_transactions; ++i)
    {
      if (strcmp (ce->transactions[i]->id, id) == 0)
        {
          index = i;
          break;
        }
    }
  if (index >= 0)
    {
      casheph_trn_destroy (ce->transactions[index]);
      int j;
      for (j = index; j < ce->n_transactions - 1; ++j)
        {
          ce->transactions[j] = ce->transactions[j + 1];
        }
      --ce->n_transactions;
    }
}

char *
make_guid ()
{
  char buf[2];
  char *id = (char*)malloc (33);
  int i;
  for (i = 0; i < 32; ++i)
    {
      char v = rand () % 16;
      sprintf (buf, "%x", v);
      id[i] = buf[0];
    }
  return id;
}

casheph_val_t *
casheph_copy_val (casheph_val_t *val)
{
  casheph_val_t *val_cpy = (casheph_val_t*)malloc (sizeof (casheph_val_t));
  val_cpy->n = val->n;
  val_cpy->d = val->d;
  return val_cpy;
}

casheph_transaction_t *
casheph_add_simple_trn (casheph_t *ce, casheph_account_t *from,
                        casheph_account_t *to, casheph_gdate_t *date,
                        casheph_val_t *val, const char *desc)
{
  srand (time (NULL));
  casheph_transaction_t *trn;
  trn = (casheph_transaction_t*)malloc (sizeof (casheph_transaction_t));
  trn->id = make_guid ();
  struct tm tm;
  tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
  tm.tm_mday = date->day;
  tm.tm_mon = date->month - 1;
  tm.tm_year = date->year - 1900;
  tm.tm_isdst = -1;
  trn->date_posted = mktime (&tm);
  trn->date_entered = time (NULL);
  trn->desc = (char*)malloc (strlen (desc) + 1);
  strcpy (trn->desc, desc);
  trn->n_slots = 1;
  trn->slots = (casheph_slot_t**)malloc (sizeof (casheph_slot_t*));
  trn->slots[0] = (casheph_slot_t*)malloc (sizeof (casheph_slot_t));
  trn->slots[0]->key = (char*)malloc (12);
  strcpy (trn->slots[0]->key, "date-posted");
  trn->slots[0]->type = ce_gdate;
  casheph_gdate_t *date_cpy = (casheph_gdate_t*)malloc (sizeof (casheph_gdate_t));
  date_cpy->year = date->year;
  date_cpy->month = date->month;
  date_cpy->day = date->day;
  trn->slots[0]->value = date_cpy;
  trn->n_splits = 2;
  trn->splits = (casheph_split_t**)malloc (sizeof (casheph_split_t*) * 2);
  trn->splits[0] = (casheph_split_t*)malloc (sizeof (casheph_split_t));
  trn->splits[0]->id = make_guid ();
  trn->splits[0]->reconciled_state = (char*)malloc (2);
  strcpy (trn->splits[0]->reconciled_state, "n");
  casheph_val_t *v0 = casheph_copy_val (val);
  casheph_val_t *q0 = casheph_copy_val (val);
  trn->splits[0]->value = v0;
  trn->splits[0]->quantity = q0;
  trn->splits[0]->account = (char*)malloc (strlen (to->id) + 1);
  strcpy (trn->splits[0]->account, to->id);
  trn->splits[0]->n_slots = 0;
  trn->splits[0]->slots = NULL;

  trn->splits[1] = (casheph_split_t*)malloc (sizeof (casheph_split_t));
  trn->splits[1]->id = make_guid ();
  trn->splits[1]->reconciled_state = (char*)malloc (2);
  strcpy (trn->splits[1]->reconciled_state, "n");
  casheph_val_t *v1 = casheph_copy_val (val);
  v1->n *= -1;
  casheph_val_t *q1 = casheph_copy_val (val);
  trn->splits[1]->value = v1;
  trn->splits[1]->quantity = q1;
  trn->splits[1]->account = (char*)malloc (strlen (from->id) + 1);
  strcpy (trn->splits[1]->account, from->id);
  trn->splits[1]->n_slots = 0;
  trn->splits[1]->slots = NULL;
  ++ce->n_transactions;
  ce->transactions[ce->n_transactions - 1] = trn;
  return trn;
}

casheph_account_t *
casheph_get_account_rec (casheph_account_t *act, const char *id)
{
  if (strcmp (act->id, id) == 0)
    {
      return act;
    }
  int i;
  for (i = 0; i < act->n_accounts; ++i)
    {
      casheph_account_t *inner = casheph_get_account_rec (act->accounts[i], id);
      if (inner != NULL)
        {
          return inner;
        }
    }
  return NULL;
}

casheph_account_t *
casheph_get_account (casheph_t *ce, const char *id)
{
  return casheph_get_account_rec (ce->root, id);
}
