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

casheph_slot_t *casheph_parse_slot_contents (gzFile file);

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
casheph_parse_simple_int_tag (int *dest, casheph_tag_t **tag, const char *name, gzFile file)
{
  char *buf = NULL;
  casheph_parse_simple_complete_tag (&buf, tag, name, file);
  if (buf != NULL)
    {
      sscanf (buf, "%d", dest);
      free (buf);
      buf = NULL;
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

casheph_gdate_t *
casheph_parse_gdate (gzFile file)
{
  casheph_gdate_t *date = (casheph_gdate_t*)malloc (sizeof (casheph_gdate_t));
  casheph_tag_t *tag = casheph_parse_tag (file);
  if (strcmp (tag->name, "gdate") != 0)
    {
      fprintf (stderr, "gdate tag not found in slot of type gdate\n");
    }
  char *buf = casheph_parse_text (file);
  date->year = (buf[0] - '0') * 1000 + (buf[1] - '0') * 1000
    + (buf[2] - '0') * 10 + (buf[3] - '0');
  date->month = (buf[5] - '0') * 10 + (buf[6] - '0');
  date->day = (buf[8] - '0') * 10 + (buf[9] - '0');
  casheph_tag_destroy (tag);
  tag = casheph_parse_tag (file);
  if (strcmp (tag->name, "/gdate") != 0)
    {
      fprintf (stderr, "Couldn't find matching </gdate>\n");
      exit (1);
    }
  casheph_tag_destroy (tag);
  return date;
}

uint64_t
casheph_parse_value_str (const char *str)
{
  uint64_t val;
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
      sscanf (str, "%d/%d", &n, &d);
      val = ((uint64_t)d << 32) | (uint32_t)n;
    }
  else
    {
      sscanf (str, "%ld", &val);
      val *= 100;
    }
  return val;
}

void
casheph_parse_slot_value (void **value, casheph_slot_type_t *type, casheph_tag_t **tag, gzFile file)
{
  if (*tag != NULL && strcmp ((*tag)->name, "slot:value") == 0)
    {
      if ((*tag)->n_attributes > 0
          && strcmp ((*tag)->attributes[0]->key, "type") == 0
          && strcmp ((*tag)->attributes[0]->val, "gdate") == 0)
        {
          *type = ce_gdate;
          *value = casheph_parse_gdate (file);
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
        }
      else if ((*tag)->n_attributes > 0
               && strcmp ((*tag)->attributes[0]->key, "type") == 0
               && strcmp ((*tag)->attributes[0]->val, "string") == 0)
        {
          *type = ce_string;
          char *buf = casheph_parse_text (file);
          *value = buf;
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
        }
      else if ((*tag)->n_attributes > 0
               && strcmp ((*tag)->attributes[0]->key, "type") == 0
               && strcmp ((*tag)->attributes[0]->val, "guid") == 0)
        {
          *type = ce_guid;
          char *buf = casheph_parse_text (file);
          *value = buf;
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
        }
      else if ((*tag)->n_attributes > 0
               && strcmp ((*tag)->attributes[0]->key, "type") == 0
               && strcmp ((*tag)->attributes[0]->val, "numeric") == 0)
        {
          *type = ce_numeric;
          char *buf = casheph_parse_text (file);
          *value = (long*)malloc (sizeof (long));
          *((long*)*value) = casheph_parse_value_str (buf);
          free (buf);
          casheph_tag_destroy (*tag);
          *tag = casheph_parse_tag (file);
        }
      else if ((*tag)->n_attributes > 0
               && strcmp ((*tag)->attributes[0]->key, "type") == 0
               && strcmp ((*tag)->attributes[0]->val, "frame") == 0)
        {
          *type = ce_frame;
          casheph_frame_t *frame = (casheph_frame_t*)malloc (sizeof (casheph_frame_t));
          frame->n_slots = 0;
          frame->slots = NULL;
          *tag = casheph_parse_tag (file);
          while (strcmp ((*tag)->name, "slot") == 0)
            {
              casheph_slot_t *slot = casheph_parse_slot_contents (file);
              ++frame->n_slots;
              frame->slots = (casheph_slot_t**)realloc (frame->slots, sizeof (casheph_slot_t*) * frame->n_slots);
              frame->slots[frame->n_slots - 1] = slot;
              casheph_tag_destroy (*tag);
              *tag = casheph_parse_tag (file);
            }
          *value = frame;
        }
      if (strcmp ((*tag)->name, "/slot:value") != 0)
        {
          fprintf (stderr, "Couldn't find matching </slot:value>\n");
          fprintf (stderr, "Instead, found <%s>\n", (*tag)->name);
          exit (1);
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
    }
}

casheph_slot_t *
casheph_parse_slot_contents (gzFile file)
{
  casheph_slot_t *slot = (casheph_slot_t*)malloc (sizeof (casheph_slot_t));
  slot->key = NULL;
  slot->value = NULL;
  casheph_tag_t *slot_tag = casheph_parse_tag (file);
  while (strcmp (slot_tag->name, "/slot") != 0)
    {
      casheph_parse_simple_complete_tag (&(slot->key), &slot_tag, "slot:key", file);
      casheph_parse_slot_value (&(slot->value), &(slot->type), &slot_tag, file);
      casheph_skip_any_tag (&slot_tag, file);
      casheph_tag_destroy (slot_tag);
      slot_tag = casheph_parse_tag (file);
    }
  if (strcmp (slot_tag->name, "/slot") != 0)
    {
      fprintf (stderr, "Couldn't find matching </slot>\n");
      fprintf (stderr, "Instead, found <%s>\n", slot_tag->name);
      exit (1);
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
          fprintf (stderr, "Couldn't find matching <%s>\n", endtagname);
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

void
casheph_parse_commodity (casheph_commodity_t **commodity_ptr, casheph_tag_t **tag, const char *tagname, gzFile file)
{
  char *endtagname = (char*)malloc (strlen (tagname) + 2);
  sprintf (endtagname, "/%s", tagname);
  if (*tag != NULL && strcmp ((*tag)->name, tagname) == 0)
    {
      casheph_commodity_t *commodity = (casheph_commodity_t*)malloc (sizeof (casheph_commodity_t));
      commodity->space = NULL;
      commodity->id = NULL;
      casheph_tag_destroy (*tag);
      *tag = casheph_parse_tag (file);
      while (strcmp ((*tag)->name, endtagname) != 0)
        {
          casheph_parse_simple_complete_tag (&(commodity->space), tag, "cmdty:space", file);
          casheph_parse_simple_complete_tag (&(commodity->id), tag, "cmdty:id", file);
          casheph_skip_any_tag (tag, file);
          *tag = casheph_parse_tag (file);
        }
      if (strcmp ((*tag)->name, endtagname) != 0)
        {
          fprintf (stderr, "Couldn't find matching <%s>\n", endtagname);
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
      *commodity_ptr = commodity;
    }
  free (endtagname);
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
  account->commodity = NULL;
  char *cmdty_scu = NULL;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:account") != 0)
    {
      casheph_parse_simple_complete_tag (&(account->name), &tag, "act:name", file);
      casheph_parse_simple_complete_tag (&(account->type), &tag, "act:type", file);
      casheph_parse_simple_complete_tag (&(account->id), &tag, "act:id", file);
      casheph_parse_simple_complete_tag (&(account->description), &tag, "act:description", file);
      casheph_parse_simple_complete_tag (&(account->parent), &tag, "act:parent", file);
      casheph_parse_account_slots (&(account->slots), &(account->n_slots), &tag, file);
      casheph_parse_commodity (&(account->commodity), &tag, "act:commodity", file);
      casheph_parse_simple_complete_tag (&cmdty_scu, &tag, "act:commodity-scu", file);
      if (cmdty_scu != NULL)
        {
          sscanf (cmdty_scu, "%d", &(account->commodity_scu));
          free (cmdty_scu);
          cmdty_scu = NULL;
        }
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

void
casheph_parse_split_slots (casheph_slot_t ***slots, int *n_slots, casheph_tag_t **tag, gzFile file)
{
  casheph_parse_slots ("split:", slots, n_slots, tag, file);
}

casheph_split_t *
casheph_parse_split_contents (gzFile file)
{
  casheph_split_t *split = (casheph_split_t*)malloc (sizeof (casheph_split_t));
  split->id = NULL;
  split->reconciled_state = NULL;
  split->account = NULL;
  split->value = 0;
  split->quantity = 0;
  split->slots = NULL;
  split->n_slots = 0;
  casheph_tag_t *split_tag = casheph_parse_tag (file);
  while (strcmp (split_tag->name, "/trn:split") != 0)
    {
      casheph_parse_simple_complete_tag (&(split->id), &split_tag, "split:id", file);
      casheph_parse_simple_complete_tag (&(split->reconciled_state), &split_tag, "split:reconciled-state", file);
      casheph_parse_simple_complete_tag (&(split->account), &split_tag, "split:account", file);
      casheph_parse_split_slots (&(split->slots), &(split->n_slots), &split_tag, file);
      if (split_tag != NULL && strcmp (split_tag->name, "split:value") == 0)
        {
          char *buf = casheph_parse_text (file);
          uint64_t val = casheph_parse_value_str (buf);
          split->value = val;
          casheph_tag_destroy (split_tag);
          split_tag = casheph_parse_tag (file);
          if (strcmp (split_tag->name, "/split:value") != 0)
            {
              fprintf (stderr, "Couldn't find matching </split:value>\n");
              exit (1);
            }
        }
      if (split_tag != NULL && strcmp (split_tag->name, "split:quantity") == 0)
        {
          char *buf = casheph_parse_text (file);
          uint64_t val = casheph_parse_value_str (buf);
          split->quantity = val;
          casheph_tag_destroy (split_tag);
          split_tag = casheph_parse_tag (file);
          if (strcmp (split_tag->name, "/split:quantity") != 0)
            {
              fprintf (stderr, "Couldn't find matching </split:quantity>\n");
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
casheph_parse_simple_bool_tag (bool *out, casheph_tag_t **tag, const char *tagname, gzFile file)
{
  char *temp = NULL;
  casheph_parse_simple_complete_tag (&temp, tag, tagname, file);
  if (temp != NULL)
    {
      *out = strcmp (temp, "y") == 0;
      free (temp);
      temp = NULL;
    }
}

casheph_recurrence_t *
casheph_parse_recurrence (casheph_tag_t **tag, gzFile file)
{
  casheph_recurrence_t *ret = NULL;
  if (*tag != NULL && strcmp ((*tag)->name, "gnc:recurrence") == 0)
    {
      casheph_recurrence_t *recurrence = (casheph_recurrence_t*)malloc (sizeof (casheph_recurrence_t));
      recurrence->mult = 0;
      recurrence->period_type = NULL;
      recurrence->start = NULL;
      recurrence->weekend_adj = NULL;
      *tag = casheph_parse_tag (file);
      while (strcmp ((*tag)->name, "/gnc:recurrence") != 0)
        {
          casheph_parse_simple_int_tag (&(recurrence->mult), tag, "recurrence:mult", file);
          casheph_parse_simple_complete_tag (&(recurrence->period_type), tag, "recurrence:period_type", file);
          casheph_parse_simple_complete_tag (&(recurrence->weekend_adj), tag, "recurrence:weekend_adj", file);
          if (*tag != NULL && strcmp ((*tag)->name, "recurrence:start") == 0)
            {
              recurrence->start = casheph_parse_gdate (file);
              casheph_tag_destroy (*tag);
              *tag = NULL;
            }
          casheph_skip_any_tag (tag, file);
          *tag = casheph_parse_tag (file);
        }
      if (strcmp ((*tag)->name, "/gnc:recurrence") != 0)
        {
          fprintf (stderr, "Couldn't find matching </gnc:recurrence>\n");
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
      ret = recurrence;
    }
  return ret;
}

void
casheph_parse_schedule (casheph_schedule_t **schedule_ptr, casheph_tag_t **tag, const char *tagname, gzFile file)
{
  char *endtagname = (char*)malloc (strlen (tagname) + 2);
  sprintf (endtagname, "/%s", tagname);
  if (*tag != NULL && strcmp ((*tag)->name, tagname) == 0)
    {
      casheph_schedule_t *schedule = (casheph_schedule_t*)malloc (sizeof (casheph_schedule_t));
      schedule->n_recurrences = 0;
      schedule->recurrences = NULL;
      *tag = casheph_parse_tag (file);
      while (strcmp ((*tag)->name, endtagname) != 0)
        {
          casheph_recurrence_t *rec = casheph_parse_recurrence (tag, file);
          if (rec != NULL)
            {
              ++schedule->n_recurrences;
              schedule->recurrences = (casheph_recurrence_t**)realloc (schedule->recurrences, sizeof (casheph_recurrence_t*) * schedule->n_recurrences);
              schedule->recurrences[schedule->n_recurrences - 1] = rec;
            }
          casheph_skip_any_tag (tag, file);
          *tag = casheph_parse_tag (file);
        }
      if (strcmp ((*tag)->name, endtagname) != 0)
        {
          fprintf (stderr, "Couldn't find matching <%s>\n", endtagname);
        }
      casheph_tag_destroy (*tag);
      *tag = NULL;
      *schedule_ptr = schedule;
    }
  free (endtagname);
}

casheph_schedxaction_t *
casheph_parse_schedxaction_contents (gzFile file)
{
  casheph_schedxaction_t *sx = (casheph_schedxaction_t*)malloc (sizeof (casheph_schedxaction_t));
  sx->id = NULL;
  sx->start = NULL;
  sx->last = NULL;
  sx->schedule = NULL;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:schedxaction") != 0)
    {
      casheph_parse_simple_complete_tag (&(sx->id), &tag, "sx:id", file);
      casheph_parse_simple_complete_tag (&(sx->name), &tag, "sx:name", file);
      casheph_parse_simple_bool_tag (&(sx->enabled), &tag, "sx:enabled", file);
      casheph_parse_simple_bool_tag (&(sx->auto_create), &tag, "sx:autoCreate", file);
      casheph_parse_simple_bool_tag (&(sx->auto_create_notify), &tag, "sx:autoCreateNotify", file);
      casheph_parse_simple_int_tag (&(sx->advance_create_days), &tag, "sx:advanceCreateDays", file);
      casheph_parse_simple_int_tag (&(sx->advance_remind_days), &tag, "sx:advanceRemindDays", file);
      casheph_parse_simple_int_tag (&(sx->instance_count), &tag, "sx:instanceCount", file);
      if (tag != NULL && strcmp (tag->name, "sx:start") == 0)
        {
          sx->start = casheph_parse_gdate (file);
          casheph_tag_destroy (tag);
          tag = NULL;
        }
      if (tag != NULL && strcmp (tag->name, "sx:last") == 0)
        {
          sx->last = casheph_parse_gdate (file);
          casheph_tag_destroy (tag);
          tag = NULL;
        }
      casheph_parse_simple_complete_tag (&(sx->templ_acct), &tag, "sx:templ-acct", file);
      casheph_parse_schedule (&(sx->schedule), &tag, "sx:schedule", file);
      casheph_skip_any_tag (&tag, file);
      tag = casheph_parse_tag (file);
    }
  casheph_tag_destroy (tag);
  return sx;
}

void
parse_template_transactions (casheph_account_t **template_root,
                             int *n_template_transactions,
                             casheph_transaction_t ***template_transactions,
                             gzFile file)
{
  int n_accounts = 0;
  casheph_account_t **accounts = NULL;
  casheph_tag_t *tag = casheph_parse_tag (file);
  while (strcmp (tag->name, "/gnc:template-transactions") != 0)
    {
      if (strcmp (tag->name, "gnc:account") == 0)
        {
          casheph_account_t *act = parse_account_contents (file);
          if (act != NULL)
            {
              ++n_accounts;
              accounts = (casheph_account_t**)realloc (accounts, sizeof (casheph_account_t*) * n_accounts);
              accounts[n_accounts - 1] = act;
              if (strcmp (act->type, "ROOT") == 0)
                {
                  *template_root = act;
                }
            }
        }
      else if (strcmp (tag->name, "gnc:transaction") == 0)
        {
          casheph_transaction_t *trn = casheph_parse_trn_contents (file);
          ++(*n_template_transactions);
          *template_transactions = (casheph_transaction_t**)realloc (*template_transactions,
                                                                     sizeof (casheph_transaction_t*) * *n_template_transactions);
          (*template_transactions)[*n_template_transactions - 1] = trn;
        }
      casheph_tag_destroy (tag);
      tag = casheph_parse_tag (file);
    }
  casheph_account_collect_accounts (*template_root, n_accounts, accounts);
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
  ce->template_root = NULL;
  ce->n_schedxactions = 0;
  ce->schedxactions = NULL;
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
          parse_template_transactions (&(ce->template_root),
                                       &(ce->n_template_transactions),
                                       &(ce->template_transactions),
                                       file);
        }
      if (tag != NULL && strcmp (tag->name, "gnc:schedxaction") == 0)
        {
          casheph_tag_destroy (tag);
          tag = NULL;
          ++ce->n_schedxactions;
          casheph_schedxaction_t *sx = casheph_parse_schedxaction_contents (file);
          ce->schedxactions = (casheph_schedxaction_t**)realloc (ce->schedxactions,
                                                                 sizeof (casheph_schedxaction_t*) * ce->n_schedxactions);
          ce->schedxactions[ce->n_schedxactions - 1] = sx;
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
                *((uint64_t*)slot->value) & 0xFFFFFFFF,
                (int32_t)(*((uint64_t*)slot->value) >> 32));
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
          gzprintf (file, "      <split:value>%d/%d</split:value>\n",
                    trn->splits[i]->value & 0xFFFFFFFF,
                    (int32_t)(trn->splits[i]->value >> 32));
          gzprintf (file, "      <split:quantity>%d/%d</split:quantity>\n",
                    trn->splits[i]->quantity & 0xFFFFFFFF,
                    (int32_t)(trn->splits[i]->quantity >> 32));
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

uint64_t
casheph_trn_value_for_act (casheph_transaction_t *trn, casheph_account_t *act)
{
  uint64_t val = 0;
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
