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

casheph_account_t *
parse_account_contents (gzFile file)
{
  casheph_account_t *account = (casheph_account_t*)malloc (sizeof (casheph_account_t));
  account->name = NULL;
  account->type = NULL;
  account->accounts = NULL;
  account->n_accounts = 0;
  account->id = NULL;
  account->parent = NULL;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:account") != 0)
    {
      casheph_parse_simple_complete_tag (&(account->name), &tag, "act:name", file);
      casheph_parse_simple_complete_tag (&(account->type), &tag, "act:type", file);
      casheph_parse_simple_complete_tag (&(account->id), &tag, "act:id", file);
      casheph_parse_simple_complete_tag (&(account->parent), &tag, "act:parent", file);
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
      casheph_parse_simple_complete_tag (&(split->reconciled_state), &split_tag, "split:reconciled_state", file);
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
casheph_parse_date_posted (time_t *date_posted, casheph_tag_t **tag, gzFile file)
{
  if ((*tag) != NULL && strcmp ((*tag)->name, "trn:date-posted") == 0)
    {
      casheph_tag_t *date_tag = casheph_parse_tag (file);
      if (strcmp (date_tag->name, "ts:date") != 0)
        {
          fprintf (stderr, "No ts:date found\n");
        }
      char date_posted_str[25];
      if (gzread (file, date_posted_str, 25) < 25)
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
          strptime (date_posted_str, "%Y-%m-%d %H:%M:%S", &tm);
          (*date_posted) = mktime (&tm);
          (*date_posted) += tm.tm_gmtoff;
          int minutes_to_add = date_posted_str[24] - '0'
            + (date_posted_str[23] - '0') * 10
            + (date_posted_str[22] - '0') * 60
            + (date_posted_str[21] - '0') * 600;
          int seconds_to_add = minutes_to_add * 60;
          if (date_posted_str[20] == '+')
            {
              seconds_to_add *= -1;
            }
          (*date_posted) += seconds_to_add;
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
          if (strcmp ((*tag)->name, "/trn:date-posted") != 0)
            {
              fprintf (stderr, "Couldn't find matching </trn:date-posted>\n");
              exit (1);
            }
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
    }
}

casheph_transaction_t *
casheph_parse_trn_contents (gzFile file)
{
  casheph_transaction_t *trn = (casheph_transaction_t*)malloc (sizeof (casheph_transaction_t));
  trn->n_splits = 0;
  trn->splits = NULL;
  trn->id = NULL;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:transaction") != 0)
    {
      casheph_parse_simple_complete_tag (&(trn->id), &tag, "trn:id", file);
      casheph_parse_simple_complete_tag (&(trn->desc), &tag, "trn:description", file);
      casheph_parse_splits (&(trn->splits), &(trn->n_splits), &tag, file);
      casheph_parse_date_posted (&(trn->date_posted), &tag, file);
      casheph_skip_any_tag (&tag, file);
      tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (tag);
  return trn;
}

void
parse_template_transactions (gzFile file)
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
  int n_accounts = 0;
  casheph_account_t **accounts = NULL;
  while (!gzeof (file))
    {
      casheph_skip_text (file);
      casheph_tag_t *tag = casheph_parse_tag (file);
      casheph_account_t *account = NULL;
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
          parse_template_transactions (file);
        }
      casheph_tag_destroy (tag);
    }
  casheph_account_collect_accounts (ce->root, n_accounts, accounts);
  gzclose (file);
  return ce;
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
