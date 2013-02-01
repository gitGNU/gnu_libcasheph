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

#include "casheph.h"

void
casheph_consume_whitespace (int *c, gzFile file)
{
  do
    {
      *c = gzgetc (file);
    }
  while (*c != -1 && (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r'));
}

casheph_attribute_t *
casheph_parse_attribute (gzFile file)
{
  int c;
  casheph_consume_whitespace (&c, file);
  if (c == -1)
    {
      return NULL;
    }
  char *name = (char*)malloc (256);
  char *cur = name;
  do
    {
      *cur = c;
      cur++;
      *cur = '\0';
      c = gzgetc (file);
    }
  while (c != -1 && c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '=');

  casheph_attribute_t *att = (casheph_attribute_t*)malloc (sizeof (casheph_attribute_t));
  att->key = (char*)malloc (cur - name + 1);
  strcpy (att->key, name);

  if (c != '=')
    {
      casheph_consume_whitespace (&c, file);
      if (c != '=')
        {
          free (name);
          free (att->key);
          free (att);
          return NULL;
        }
    }
  casheph_consume_whitespace (&c, file);
  if (c == -1)
    {
      return NULL;
    }
  cur = name;
  if (c == '\"')
    {
      do
        {
          *cur = c;
          cur++;
          *cur = '\0';
          c = gzgetc (file);
        }
      while (c != -1 && c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\"');
    }
  else if (c == '\'')
    {
      do
        {
          *cur = c;
          cur++;
          *cur = '\0';
          c = gzgetc (file);
        }
      while (c != -1 && c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\'');
    }

  att->val = (char*)malloc (cur - name);
  strcpy (att->val, name + 1);
  free (name);
  return att;
}

casheph_tag_t *
casheph_parse_tag (gzFile file)
{
  int c;
  casheph_consume_whitespace (&c, file);
  if (c == -1)
    {
      return NULL;
    }
  if (c != '<')
    {
      return NULL;
    }
  char *name = (char*)malloc (256);
  char *cur = name;
  do
    {
      *cur = c;
      cur++;
      *cur = '\0';
      c = gzgetc (file);
    }
  while (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '>');
  casheph_tag_t *tag = (casheph_tag_t*)malloc (sizeof (casheph_tag_t));
  tag->name = (char*)malloc (cur - name);
  strcpy (tag->name, name + 1);
  free (name);
  if (strcmp (tag->name, "!--") == 0)
    {
      free (tag->name);
      free (tag);
      return NULL;
    }
  tag->n_attributes = 0;
  tag->attributes = NULL;
  while (c != '>')
    {
      ++tag->n_attributes;
      tag->attributes = (casheph_attribute_t**)realloc (tag->attributes, tag->n_attributes * sizeof (casheph_attribute_t*));
      tag->attributes[tag->n_attributes - 1] = casheph_parse_attribute (file);
      c = gzgetc (file);
    }
  return tag;
}

void
casheph_tag_destroy (casheph_tag_t *tag)
{
  if (tag == NULL)
    {
      return;
    }
  int i;
  for (i = 0; i < tag->n_attributes; ++i)
    {
      free (tag->attributes[i]->key);
      free (tag->attributes[i]->val);
      free (tag->attributes[i]);
    }
  free (tag->attributes);
  free (tag->name);
  free (tag);
}

void
casheph_skip_text (gzFile file)
{
  int c = gzgetc (file);
  while (c != -1 && c != '<')
    {
      c = gzgetc (file);
    }
  if (c == '<')
    {
      gzungetc ('<', file);
    }
}

char *
casheph_parse_text (gzFile file)
{
  char *name = malloc (256);
  char *cur = name;
  int c = gzgetc (file);
  while (c != -1 && c != '<')
    {
      *cur = c;
      ++cur;
      c = gzgetc (file);
    }
  *cur = '\0';
  if (c == '<')
    {
      gzungetc ('<', file);
    }
  return name;
}

void
casheph_parse_simple_complete_tag (char **dest, casheph_tag_t **tag, const char *name, gzFile file)
{
  if (*tag != NULL)
    {
      char *val = NULL;
      char *endtagname = (char*)malloc (strlen (name) + 2);
      endtagname[0] = '/';
      strcpy (endtagname + 1, name);
      if (strcmp ((*tag)->name, name) == 0)
        {
          *dest = casheph_parse_text (file);
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
          if (strcmp ((*tag)->name, endtagname) != 0)
            {
              fprintf (stderr, "Couldn't find matching <%s>\n", endtagname);
              exit (1);
            }
          casheph_tag_destroy (*tag);
          *tag = NULL;
        }
      free (endtagname);
    }
}

void
casheph_skip_any_tag (casheph_tag_t **tag, gzFile file)
{
  if (*tag != NULL && (*tag)->name[0] != '/')
    {
      char *endtagname = (char*)malloc (strlen ((*tag)->name) + 2);
      sprintf (endtagname, "/%s", (*tag)->name);
      casheph_skip_text (file);
      casheph_tag_t *tag2 = casheph_parse_tag (file);
      while (strcmp (tag2->name, endtagname) != 0)
        {
          casheph_skip_text (file);
          casheph_tag_destroy (tag2);
          tag2 = casheph_parse_tag (file);
        }
      free (endtagname);
    }
}

casheph_slot_t *
casheph_parse_slot_contents (gzFile file)
{
  casheph_slot_t *slot = (casheph_slot_t*)malloc (sizeof (casheph_slot_t));
  slot->key = NULL;
  slot->type = gdate;
  slot->value = NULL;
  casheph_tag_t *slot_tag = casheph_parse_tag (file);
  while (strcmp (slot_tag->name, "/slot") != 0)
    {
      casheph_parse_simple_complete_tag (&(slot->key), &slot_tag, "slot:key", file);
      if (slot_tag != NULL && strcmp (slot_tag->name, "slot:value") == 0)
        {
          if (strcmp (slot_tag->attributes[0]->key, "type") == 0
              && strcmp (slot_tag->attributes[0]->val, "gdate") == 0)
            {
              casheph_gdate_t *date;
              date = (casheph_gdate_t*)malloc (sizeof (casheph_gdate_t));
              slot_tag = casheph_parse_tag (file);
              if (strcmp (slot_tag->name, "gdate") != 0)
                {
                  fprintf (stderr, "gdate tag not found in slot of type gdate\n");
                }
              char *buf = casheph_parse_text (file);
              date->year = (buf[0] - '0') * 1000 + (buf[1] - '0') * 1000
                + (buf[2] - '0') * 10 + (buf[3] - '0');
              date->month = (buf[5] - '0') * 10 + (buf[6] - '0');
              date->day = (buf[8] - '0') * 10 + (buf[9] - '0');
              slot->value = date;
              casheph_tag_destroy (slot_tag);
              slot_tag = casheph_parse_tag (file);
              if (strcmp (slot_tag->name, "/gdate") != 0)
                {
                  fprintf (stderr, "Couldn't find matching </gdate>\n");
                  exit (1);
                }
              casheph_tag_destroy (slot_tag);
              slot_tag = casheph_parse_tag (file);
              if (strcmp (slot_tag->name, "/slot:value") != 0)
                {
                  fprintf (stderr, "Couldn't find matching </slot:value>\n");
                  exit (1);
                }
              casheph_tag_destroy (slot_tag);
              slot_tag = NULL;
            }
        }
      casheph_skip_any_tag (&slot_tag, file);
      slot_tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (slot_tag);
  return slot;
}

void
casheph_parse_slots (const char *prefix, casheph_slot_t ***slots, int *n_slots, casheph_tag_t **tag, gzFile file)
{
  char *tagname = (char*)malloc (strlen (prefix) + 6);
  sprintf (tagname, "%sslots", prefix);
  char *endtagname = (char*)malloc (strlen (prefix) + 7);
  sprintf (endtagname, "/%sslots", prefix);
  if (*tag != NULL && strcmp ((*tag)->name, tagname) == 0)
    {
      *n_slots = 0;
      casheph_tag_t *slot_tag = casheph_parse_tag (file);
      while (strcmp (slot_tag->name, "slot") == 0)
        {
          casheph_slot_t *slot = casheph_parse_slot_contents (file);
          ++(*n_slots);
          (*slots) = (casheph_slot_t**)realloc ((*slots), sizeof (casheph_slot_t*) * (*n_slots));
          (*slots)[(*n_slots) - 1] = slot;
          casheph_tag_destroy (slot_tag);
          slot_tag = casheph_parse_tag (file);
        }
      if (strcmp (slot_tag->name, endtagname) != 0)
        {
          fprintf (stderr, "Couldn't find matching </trn:slots>\n");
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
    }
  free (tagname);
  free (endtagname);
}

void
casheph_parse_account_slots (casheph_slot_t ***slots, int *n_slots, casheph_tag_t **tag, gzFile file)
{
  casheph_parse_slots ("act:", slots, n_slots, tag, file);
}

casheph_account_t *
parse_account_contents (gzFile file)
{
  casheph_account_t *account = (casheph_account_t*)malloc (sizeof (casheph_account_t));
  account->name = NULL;
  account->description = NULL;
  account->type = NULL;
  account->accounts = NULL;
  account->n_accounts = 0;
  account->id = NULL;
  account->parent = NULL;
  account->slots = NULL;
  account->n_slots = 0;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:account") != 0)
    {
      casheph_parse_simple_complete_tag (&(account->name), &tag, "act:name", file);
      casheph_parse_simple_complete_tag (&(account->type), &tag, "act:type", file);
      casheph_parse_simple_complete_tag (&(account->id), &tag, "act:id", file);
      casheph_parse_simple_complete_tag (&(account->description), &tag, "act:description", file);
      casheph_parse_simple_complete_tag (&(account->parent), &tag, "act:parent", file);
      casheph_parse_account_slots (&(account->slots), &(account->n_slots), &tag, file);
      casheph_skip_any_tag (&tag, file);
      tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (tag);
  return account;
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

casheph_split_t *
casheph_parse_split_contents (gzFile file)
{
  casheph_split_t *split = (casheph_split_t*)malloc (sizeof (casheph_split_t));
  split->id = NULL;
  split->reconciled_state = NULL;
  split->account = NULL;
  split->value = 0;
  casheph_tag_t *split_tag = casheph_parse_tag (file);
  while (strcmp (split_tag->name, "/trn:split") != 0)
    {
      casheph_parse_simple_complete_tag (&(split->id), &split_tag, "split:id", file);
      casheph_parse_simple_complete_tag (&(split->reconciled_state), &split_tag, "split:reconciled-state", file);
      casheph_parse_simple_complete_tag (&(split->account), &split_tag, "split:account", file);
      if (split_tag != NULL && strcmp (split_tag->name, "split:value") == 0)
        {
          char *buf = casheph_parse_text (file);
          long val;
          sscanf (buf, "%ld", &val);
          split->value = val;
          casheph_tag_destroy (split_tag);
          split_tag = casheph_parse_tag (file);
          if (strcmp (split_tag->name, "/split:value") != 0)
            {
              fprintf (stderr, "Couldn't find matching </split:value>\n");
              exit (1);
            }
        }
      casheph_skip_any_tag (&split_tag, file);
      split_tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (split_tag);
  return split;
}

void
casheph_parse_splits (casheph_split_t ***splits, int *n_splits, casheph_tag_t **tag, gzFile file)
{
  if (*tag != NULL && strcmp ((*tag)->name, "trn:splits") == 0)
    {
      *n_splits = 0;
      casheph_tag_t *split_tag = casheph_parse_tag (file);
      while (strcmp (split_tag->name, "trn:split") == 0)
        {
          casheph_split_t *split = casheph_parse_split_contents (file);
          ++(*n_splits);
          (*splits) = (casheph_split_t**)realloc ((*splits), sizeof (casheph_split_t*) * (*n_splits));
          (*splits)[(*n_splits) - 1] = split;
          casheph_tag_destroy (split_tag);
          split_tag = casheph_parse_tag (file);
        }
      if (strcmp (split_tag->name, "/trn:splits") != 0)
        {
          fprintf (stderr, "Couldn't find matching </trn:splits>\n");
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
    }
}

void
casheph_parse_trn_slots (casheph_slot_t ***slots, int *n_slots, casheph_tag_t **tag, gzFile file)
{
  casheph_parse_slots ("trn:", slots, n_slots, tag, file);
}

void
casheph_parse_date_field (time_t *date, casheph_tag_t **tag,
                          const char *tag_name, gzFile file)
{
  if ((*tag) != NULL && strcmp ((*tag)->name, tag_name) == 0)
    {
      casheph_tag_t *date_tag = casheph_parse_tag (file);
      if (strcmp (date_tag->name, "ts:date") != 0)
        {
          fprintf (stderr, "No ts:date found\n");
        }
      char date_str[25];
      if (gzread (file, date_str, 25) < 25)
        {
          fprintf (stderr, "Couldn't read expected 25 bytes of date\n");
        }
      else
        {
          casheph_tag_destroy (date_tag);
          date_tag = casheph_parse_tag (file);
          if (strcmp (date_tag->name, "/ts:date") != 0)
            {
              fprintf (stderr, "Couldn't find matching </ts:date>\n");
            }
          casheph_tag_destroy (date_tag);
          tzset ();
          struct tm tm;
          memset (&tm, 0, sizeof (struct tm));
          strptime (date_str, "%Y-%m-%d %H:%M:%S", &tm);
          (*date) = mktime (&tm);
          (*date) += tm.tm_gmtoff;
          int minutes_to_add = date_str[24] - '0'
            + (date_str[23] - '0') * 10
            + (date_str[22] - '0') * 60
            + (date_str[21] - '0') * 600;
          int seconds_to_add = minutes_to_add * 60;
          if (date_str[20] == '+')
            {
              seconds_to_add *= -1;
            }
          (*date) += seconds_to_add;
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
          char *endtagname = (char*)malloc (strlen (tag_name) + 2);
          strcpy (endtagname + 1, tag_name);
          endtagname[0] = '/';
          if (strcmp ((*tag)->name, endtagname) != 0)
            {
              fprintf (stderr, "Couldn't find matching <%s>\n", endtagname);
              exit (1);
            }
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
    }
}

void
casheph_parse_date_posted (time_t *date_posted, casheph_tag_t **tag, gzFile file)
{
  casheph_parse_date_field (date_posted, tag, "trn:date-posted", file);
}

void
casheph_parse_date_entered (time_t *date_posted, casheph_tag_t **tag, gzFile file)
{
  casheph_parse_date_field (date_posted, tag, "trn:date-entered", file);
}

casheph_transaction_t *
casheph_parse_trn_contents (gzFile file)
{
  casheph_transaction_t *trn = (casheph_transaction_t*)malloc (sizeof (casheph_transaction_t));
  trn->n_splits = 0;
  trn->splits = NULL;
  trn->n_slots = 0;
  trn->slots = NULL;
  trn->id = NULL;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:transaction") != 0)
    {
      casheph_parse_simple_complete_tag (&(trn->id), &tag, "trn:id", file);
      casheph_parse_simple_complete_tag (&(trn->desc), &tag, "trn:description", file);
      casheph_parse_splits (&(trn->splits), &(trn->n_splits), &tag, file);
      casheph_parse_trn_slots (&(trn->slots), &(trn->n_slots), &tag, file);
      casheph_parse_date_posted (&(trn->date_posted), &tag, file);
      casheph_parse_date_entered (&(trn->date_entered), &tag, file);
      casheph_skip_any_tag (&tag, file);
      tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (tag);
  return trn;
}

void
parse_template_transactions (int *n_template_transactions,
                             casheph_transaction_t ***template_transactions,
                             gzFile file)
{
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:template-transactions") != 0)
    {
      if (strcmp (tag->name, "gnc:account") == 0)
        {
          casheph_account_t *act = parse_account_contents (file);
        }
      else if (strcmp (tag->name, "gnc:transaction") == 0)
        {
          casheph_transaction_t *trn = casheph_parse_trn_contents (file);
        }
      casheph_tag_destroy (tag);
      tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (tag);
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

void
casheph_parse_account (casheph_tag_t **tag, casheph_account_t **act, gzFile file)
{
  if (*tag != NULL && strcmp ((*tag)->name, "gnc:account") == 0)
    {
      *act = parse_account_contents (file);
      casheph_tag_destroy (*tag);
      *tag = NULL;
    }
}

casheph_t *
casheph_open (const char *filename)
{
  gzFile file = gzopen (filename, "r");
  if (file == NULL)
    {
      return NULL;
    }
  if (casheph_get_xml_declaration (file) == -1)
    {
      gzclose (file);
      return NULL;
    }
  casheph_t *ce = (casheph_t*)malloc (sizeof (casheph_t));
  ce->n_transactions = 0;
  ce->transactions = NULL;
  ce->n_template_transactions = 0;
  ce->template_transactions = NULL;
  int n_accounts = 0;
  casheph_account_t **accounts = NULL;
  while (!gzeof (file))
    {
      casheph_skip_text (file);
      casheph_tag_t *tag = casheph_parse_tag (file);
      casheph_account_t *account = NULL;
      casheph_parse_simple_complete_tag (&(ce->book_id), &tag, "book:id", file);
      casheph_parse_account (&tag, &account, file);
      if (account != NULL)
        {
          ++n_accounts;
          accounts = (casheph_account_t**)realloc (accounts, sizeof (casheph_account_t*) * n_accounts);
          accounts[n_accounts - 1] = account;
          if (strcmp (account->type, "ROOT") == 0)
            {
              ce->root = account;
            }
        }
      if (tag != NULL && strcmp (tag->name, "gnc:transaction") == 0)
        {
          casheph_tag_destroy (tag);
          tag = NULL;
          ++ce->n_transactions;
          casheph_transaction_t *trn = casheph_parse_trn_contents (file);
          ce->transactions = (casheph_transaction_t**)realloc (ce->transactions, sizeof (casheph_transaction_t*) * ce->n_transactions);
          ce->transactions[ce->n_transactions - 1] = trn;
        }
      if (tag != NULL && strcmp (tag->name, "gnc:template-transactions") == 0)
        {
          casheph_tag_destroy (tag);
          tag = NULL;
          parse_template_transactions (&(ce->n_template_transactions),
                                       &(ce->template_transactions), file);
        }
      casheph_tag_destroy (tag);
    }
  casheph_account_collect_accounts (ce->root, n_accounts, accounts);
  gzclose (file);
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
casheph_write_account (casheph_account_t *account, gzFile file)
{
  gzputs (file, "<gnc:account version=\"2.0.0\">\n");
  if (strcmp (account->type, "ROOT") == 0)
    {
      gzprintf (file, "  <act:name>%s</act:name>\n", account->name);
      gzprintf (file, "  <act:id type=\"guid\">%s</act:id>\n", account->id);
      gzprintf (file, "  <act:type>ROOT</act:type>\n");
    }
  else
    {
      gzprintf (file, "  <act:name>%s</act:name>\n", account->name);
      gzprintf (file, "  <act:id type=\"guid\">%s</act:id>\n", account->id);
      gzprintf (file, "  <act:type>%s</act:type>\n", account->type);
      gzprintf (file, "  <act:commodity>\n");
      gzprintf (file, "    <cmdty:space>ISO4217</cmdty:space>\n");
      gzprintf (file, "    <cmdty:id>USD</cmdty:id>\n");
      gzprintf (file, "  </act:commodity>\n");
      gzprintf (file, "  <act:commodity-scu>100</act:commodity-scu>\n");
      gzprintf (file, "  <act:description>%s</act:description>\n", account->description);
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
  int hrs_off = timezone / 3600;
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
          gzprintf (file, "    <slot>\n");
          gzprintf (file, "      <slot:key>%s</slot:key>\n", trn->slots[i]->key);
          if (trn->slots[i]->type == gdate)
            {
              casheph_gdate_t *date = (casheph_gdate_t*)trn->slots[i]->value;
              gzprintf (file, "      <slot:value type=\"gdate\">\n");
              gzprintf (file, "        <gdate>%04d-%02d-%02d</gdate>\n",
                        date->year, date->month, date->day);
              gzprintf (file, "      </slot:value>\n");
            }
          gzprintf (file, "    </slot>\n");
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
          gzprintf (file, "      <split:value>%d/100</split:value>\n", trn->splits[i]->value);
          gzprintf (file, "      <split:quantity>%d/100</split:quantity>\n", trn->splits[i]->value);
          gzprintf (file, "      <split:account type=\"guid\">%s</split:account>\n", trn->splits[i]->account);
          gzprintf (file, "    </trn:split>\n");
        }
      gzprintf (file, "  </trn:splits>\n");
    }
  gzputs (file, "</gnc:transaction>\n");
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

long
casheph_trn_value_for_act (casheph_transaction_t *trn, casheph_account_t *act)
{
  long val = 0;
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

  gzputs (file, "</gnc:book>\n");
  gzputs (file, "</gnc-v2>\n");
  gzputs (file, "\n");
  gzputs (file, "<!-- Local variables: -->\n");
  gzputs (file, "<!-- mode: xml        -->\n");
  gzputs (file, "<!-- End:             -->\n");
  gzclose (file);
}
