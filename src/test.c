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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "zlib.h"

#include "casheph.h"

bool
test (bool (*func)(), const char *desc)
{
  bool res = func ();
  if (res)
    {
      printf ("[[32mPASS[0m]");
    }
  else
    {
      printf ("[[31mFAIL[0m]");
    }
  printf (" %s\n", desc);
  return res;
}

bool
opening_gives_obj_with_accounts ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  return ce != NULL;
}

bool
opening_gives_null_when_not_found ()
{
  casheph_t *ce = casheph_open ("blahblahblahdoesntexist.gnucash");
  return ce == NULL;
}

bool
opening_gives_null_when_no_xml_dec ()
{
  FILE *file = fopen ("noxmldec", "w");
  fprintf (file, "This is not an XML declaration... %s",
           "but it is more than forty characters...\n");
  fclose (file);
  system ("gzip -f noxmldec");
  system ("mv noxmldec.gz noxmldec.gnucash");
  casheph_t *ce = casheph_open ("noxmldec.gnucash");
  system ("rm noxmldec.gnucash");
  return ce == NULL;
}

bool
parsing_simple_tag ()
{
  FILE *file = fopen ("tagparse", "w");
  fprintf (file, " \t\n <foo>\t  \n");
  fclose (file);
  system ("gzip -f tagparse");
  system ("mv tagparse.gz tagparse.gnucash");
  gzFile g_file = gzopen ("tagparse.gnucash", "r");
  casheph_tag_t *tag = casheph_parse_tag (g_file);
  gzclose (g_file);
  system ("rm tagparse.gnucash");
  return tag != NULL && strcmp (tag->name, "foo") == 0;
}

bool
parsing_simple_tags ()
{
  FILE *file = fopen ("tagparse", "w");
  fprintf (file, " \t\n <foo>\t <bar>\r <blah>\t \n  <hello> \n");
  fclose (file);
  system ("gzip -f tagparse");
  system ("mv tagparse.gz tagparse.gnucash");
  gzFile g_file = gzopen ("tagparse.gnucash", "r");
  casheph_tag_t *tag = casheph_parse_tag (g_file);
  if (tag == NULL || strcmp (tag->name, "foo") != 0)
    {
      return false;
    }
  casheph_tag_destroy (tag);
  tag = casheph_parse_tag (g_file);
  if (tag == NULL || strcmp (tag->name, "bar") != 0)
    {
      return false;
    }
  casheph_tag_destroy (tag);
  tag = casheph_parse_tag (g_file);
  if (tag == NULL || strcmp (tag->name, "blah") != 0)
    {
      return false;
    }
  casheph_tag_destroy (tag);
  tag = casheph_parse_tag (g_file);
  if (tag == NULL || strcmp (tag->name, "hello") != 0)
    {
      return false;
    }
  casheph_tag_destroy (tag);
  gzclose (g_file);
  system ("rm tagparse.gnucash");
  return true;
}

bool
parsing_tag_with_attributes ()
{
  FILE *file = fopen ("tagparse", "w");
  fprintf (file, " \t\n <%s \t \r\n %s=\"%s\"\t  \r\n %s \t =\n \"%s\">\t\n",
           "foo",
           "hello",
           "world",
           "blah",
           "blah2");
  fclose (file);
  system ("gzip -f tagparse");
  system ("mv tagparse.gz tagparse.gnucash");
  gzFile g_file = gzopen ("tagparse.gnucash", "r");
  casheph_tag_t *tag = casheph_parse_tag (g_file);
  gzclose (g_file);
  bool good = false;
  if (tag != NULL && strcmp (tag->name, "foo") == 0 && tag->n_attributes == 2
      && strcmp (tag->attributes[0]->key, "hello") == 0
      && strcmp (tag->attributes[0]->val, "world") == 0
      && strcmp (tag->attributes[1]->key, "blah") == 0
      && strcmp (tag->attributes[1]->val, "blah2") == 0)
    {
      good = true;
    }
  casheph_tag_destroy (tag);
  system ("rm tagparse.gnucash");
  return good;
}

bool
parsing_attribute ()
{
  FILE *file = fopen ("attparse", "w");
  fprintf (file, " \t\n foo=\"bar\"\t  \r\n hello \t =\n 'world'>\t\n");
  fclose (file);
  system ("gzip -f attparse");
  system ("mv attparse.gz attparse.gnucash");
  gzFile g_file = gzopen ("attparse.gnucash", "r");
  casheph_attribute_t *att = casheph_parse_attribute (g_file);
  if (att == NULL
      || strcmp (att->key, "foo") != 0
      || strcmp (att->val, "bar") != 0)
    {
      free (att->key);
      free (att->val);
      free (att);
      gzclose (g_file);
      return false;
    }
  free (att->key);
  free (att->val);
  free (att);
  att = casheph_parse_attribute (g_file);
  if (att == NULL
      || strcmp (att->key, "hello") != 0
      || strcmp (att->val, "world") != 0)
    {
      free (att->key);
      free (att->val);
      free (att);
      gzclose (g_file);
      return false;
    }
  free (att->key);
  free (att->val);
  free (att);
  gzclose (g_file);
  system ("rm attparse.gnucash");
  return true;
}

bool
root_account_id ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  return strncmp (ce->root->id, "7e36774d188b3aca9a8ec99441466d51", 32) == 0;
}

bool
top_accounts_have_correct_names ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  casheph_account_t *root = ce->root;
  if (root->n_accounts != 5)
    {
      return false;
    }
  return (casheph_account_get_account_by_name (root, "Assets")
          && casheph_account_get_account_by_name (root, "Equity")
          && casheph_account_get_account_by_name (root, "Expenses")
          && casheph_account_get_account_by_name (root, "Income")
          && casheph_account_get_account_by_name (root, "Liabilities"));
}

bool
assets_has_current_assets ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  casheph_account_t *root = ce->root;
  casheph_account_t *assets;
  assets = casheph_account_get_account_by_name (root, "Assets");
  casheph_account_t *cur_assets;
  cur_assets = casheph_account_get_account_by_name (assets, "Current Assets");
  return cur_assets != NULL;
}

bool
has_five_transactions ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  return ce->n_transactions == 5;
}

bool
transaction_ids ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  return (casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9")
          && casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8")
          && casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0")
          && casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197")
          && casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457"));
}

bool
transaction_descs ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  casheph_transaction_t *t;
  t = casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9");
  if (strcmp (t->desc, "Opening Balance") != 0)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8");
  if (strcmp (t->desc, "Groceries") != 0)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0");
  if (strcmp (t->desc, "Gas") != 0)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197");
  if (strcmp (t->desc, "Paycheck") != 0)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457");
  if (strcmp (t->desc, "Save some money") != 0)
    {
      return false;
    }
  return true;
}

bool
transaction_date_posted ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  casheph_transaction_t *t;
  t = casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9");
  if (t->date_posted != 1354552859)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8");
  if (t->date_posted != 1354579200)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0");
  if (t->date_posted != 1354665600)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197");
  if (t->date_posted != 1354752000)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457");
  if (t->date_posted != 1354838400)
    {
      return false;
    }
  return true;
}

bool
transaction_date_entered ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  casheph_transaction_t *t;
  t = casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9");
  if (t->date_entered != 1354552859)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8");
  if (t->date_entered != 1354553000)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0");
  if (t->date_entered != 1354553025)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197");
  if (t->date_entered != 1354553048)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457");
  if (t->date_entered != 1354553221)
    {
      return false;
    }
  return true;
}

bool
transaction_date_posted_2 ()
{
  casheph_t *ce = casheph_open ("test2.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  casheph_transaction_t *t;
  t = casheph_get_transaction (ce, "19617feff46b55e572f36c46e6448550");
  if (t->date_posted != 1358830800)
    {
      printf ("expected: %d, got: %d\n", 1358830800, t->date_posted);
      return false;
    }
  t = casheph_get_transaction (ce, "5119dbe5428a736bf6bf77803438e19a");
  if (t->date_posted != 1358830800)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "397873682951e7e116ce9a02bdcf3f9d");
  if (t->date_posted != 1358882272)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "4dbde9dd8ca290d0c45521a5505443c3");
  if (t->date_posted != 1358917200)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "37e1f17418170067d66aef04bf5d0b6d");
  if (t->date_posted != 1359003600)
    {
      return false;
    }
  return true;
}

bool
transaction_values_for_checking ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  // Get the checking account
  casheph_account_t *root = ce->root;
  casheph_account_t *assets;
  assets = casheph_account_get_account_by_name (root, "Assets");
  casheph_account_t *cur_assets;
  cur_assets = casheph_account_get_account_by_name (assets, "Current Assets");
  casheph_account_t *checking;
  checking = casheph_account_get_account_by_name (cur_assets, "Checking Account");

  casheph_transaction_t *t;
  t = casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9");
  if (casheph_trn_value_for_act (t, checking)->n != 20000
      || casheph_trn_value_for_act (t, checking)->d != 100)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8");
  if (casheph_trn_value_for_act (t, checking)->n != -3214
      || casheph_trn_value_for_act (t, checking)->d != 100)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0");
  if (casheph_trn_value_for_act (t, checking)->n != -4823
      || casheph_trn_value_for_act (t, checking)->d != 100)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197");
  if (casheph_trn_value_for_act (t, checking)->n != 500279
      || casheph_trn_value_for_act (t, checking)->d != 100)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457");
  if (casheph_trn_value_for_act (t, checking)->n != -312766
      || casheph_trn_value_for_act (t, checking)->d != 100)
    {
      return false;
    }
  return true;
}

bool
has_one_slot_with_date_value (casheph_transaction_t *t, unsigned int year,
                              unsigned int month, unsigned int day)
{
  if (t->n_slots != 1)
    {
      return false;
    }
  if (strcmp (t->slots[0]->key, "date-posted") != 0)
    {
      return false;
    }
  if (t->slots[0]->type != ce_gdate)
    {
      return false;
    }
  casheph_gdate_t *date = (casheph_gdate_t*)t->slots[0]->value;
  if (date->year != year || date->month != month || date->day != day)
    {
      return false;
    }
  return true;
}

bool
transaction_slots ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  // Get the checking account
  casheph_account_t *root = ce->root;
  casheph_account_t *assets;
  assets = casheph_account_get_account_by_name (root, "Assets");
  casheph_account_t *cur_assets;
  cur_assets = casheph_account_get_account_by_name (assets, "Current Assets");
  casheph_account_t *checking;
  checking = casheph_account_get_account_by_name (cur_assets, "Checking Account");

  casheph_transaction_t *t;
  t = casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9");
  if (t->n_slots != 0)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8");
  if (!has_one_slot_with_date_value (t, 2012, 12, 4))
    {
      return false;
    }
  t = casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0");
  if (!has_one_slot_with_date_value (t, 2012, 12, 5))
    {
      return false;
    }
  t = casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197");
  if (!has_one_slot_with_date_value (t, 2012, 12, 6))
    {
      return false;
    }
  t = casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457");
  if (!has_one_slot_with_date_value (t, 2012, 12, 7))
    {
      return false;
    }
  return true;
}

bool
book_id_is_correct ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (strcmp (ce->book_id, "12cec244a8dd6053ebb1d461bec78f37") != 0)
    {
      return false;
    }
  return true;
}

bool
saving_produces_file_with_same_content ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  setenv ("TZ", "UTC+0", 1);
  casheph_save (ce, "test.gnucash.saved");
  system ("gunzip -c test.gnucash > test.gnucash.raw");
  system ("gunzip -c test.gnucash.saved > test.gnucash.saved.raw");
  int res = system ("diff test.gnucash.raw test.gnucash.saved.raw > test.gnucash.saved.diff");
  if (res == 0)
    {
      system ("rm test.gnucash.saved");
      system ("rm test.gnucash.raw");
      system ("rm test.gnucash.saved.raw");
      system ("rm test.gnucash.saved.diff");
    }
  return res == 0;
}

bool
saving_produces_file_with_same_content_2 ()
{
  casheph_t *ce = casheph_open ("test2.gnucash");
  setenv ("TZ", "EST+5", 1);
  casheph_save (ce, "test2.gnucash.saved");
  system ("gunzip -c test2.gnucash > test2.gnucash.raw");
  system ("gunzip -c test2.gnucash.saved > test2.gnucash.saved.raw");
  int res = system ("diff test2.gnucash.raw test2.gnucash.saved.raw > test2.gnucash.saved.diff");
  if (res == 0)
    {
      system ("rm test2.gnucash.saved");
      system ("rm test2.gnucash.raw");
      system ("rm test2.gnucash.saved.raw");
      system ("rm test2.gnucash.saved.diff");
    }
  return res == 0;
}

bool
some_have_zero_template_transactions ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_template_transactions != 0)
    {
      return false;
    }
  ce = casheph_open ("test2.gnucash");
  if (ce->n_template_transactions != 0)
    {
      return false;
    }
  return true;
}

bool
test3_has_four_template_transactions ()
{
  casheph_t *ce = casheph_open ("test3.gnucash");
  return ce->n_template_transactions == 4;
}

bool
test3_template_root_has_four_accounts ()
{
  casheph_t *ce = casheph_open ("test3.gnucash");
  return ce->template_root != NULL && ce->template_root->n_accounts == 4;
}

bool
saving_produces_file_with_same_content_3 ()
{
  casheph_t *ce = casheph_open ("test3.gnucash");
  setenv ("TZ", "America/New_York", 1);
  casheph_save (ce, "test3.gnucash.saved");
  system ("gunzip -c test3.gnucash > test3.gnucash.raw");
  system ("gunzip -c test3.gnucash.saved > test3.gnucash.saved.raw");
  int res = system ("diff test3.gnucash.raw test3.gnucash.saved.raw > test3.gnucash.saved.diff");
  if (res == 0)
    {
      system ("rm test3.gnucash.saved");
      system ("rm test3.gnucash.raw");
      system ("rm test3.gnucash.saved.raw");
      system ("rm test3.gnucash.saved.diff");
    }
  return res == 0;
}

bool
saving_produces_file_with_same_content_4 ()
{
  casheph_t *ce = casheph_open ("test4.gnucash");
  setenv ("TZ", "America/New_York", 1);
  casheph_save (ce, "test4.gnucash.saved");
  system ("gunzip -c test4.gnucash > test4.gnucash.raw");
  system ("gunzip -c test4.gnucash.saved > test4.gnucash.saved.raw");
  int res = system ("diff test4.gnucash.raw test4.gnucash.saved.raw > test4.gnucash.saved.diff");
  if (res == 0)
    {
      system ("rm test4.gnucash.saved");
      system ("rm test4.gnucash.raw");
      system ("rm test4.gnucash.saved.raw");
      system ("rm test4.gnucash.saved.diff");
    }
  return res == 0;
}

bool
test3_template_trn_splits_have_frame_slots_with_5_slots ()
{
  casheph_t *ce = casheph_open ("test3.gnucash");
  if (ce->n_template_transactions != 4)
    {
      return false;
    }
  if (ce->template_transactions[0]->n_splits != 2)
    {
      return false;
    }
  if (ce->template_transactions[0]->splits[0]->n_slots != 1)
    {
      return false;
    }
  if (ce->template_transactions[0]->splits[0]->slots[0]->type != ce_frame)
    {
      return false;
    }
  casheph_frame_t *frame;
  frame = (casheph_frame_t*)(ce->template_transactions[0]->splits[0]->slots[0]->value);
  if (frame->n_slots != 5)
    {
      return false;
    }
  if (frame->slots[0]->type != ce_guid
      || strcmp ("account", frame->slots[0]->key) != 0
      || strcmp ("014ef6a0e294480bdeffef3873e978f2", frame->slots[0]->value) != 0)
    {
      return false;
    }
  if (frame->slots[1]->type != ce_string
      || strcmp ("credit-formula", frame->slots[1]->key) != 0
      || strcmp ("2000", frame->slots[1]->value) != 0)
    {
      return false;
    }
  if (frame->slots[2]->type != ce_numeric
      || strcmp ("credit-numeric", frame->slots[2]->key) != 0
      || ((casheph_val_t*)frame->slots[2]->value)->n != 2000
      || ((casheph_val_t*)frame->slots[2]->value)->d != 1)
    {
      return false;
    }
  if (frame->slots[3]->type != ce_string
      || strcmp ("debit-formula", frame->slots[3]->key) != 0
      || strcmp ("", frame->slots[3]->value) != 0)
    {
      return false;
    }
  if (frame->slots[4]->type != ce_numeric
      || strcmp ("debit-numeric", frame->slots[4]->key)
      || ((casheph_val_t*)frame->slots[4]->value)->n != 0
      || ((casheph_val_t*)frame->slots[4]->value)->d != 1)
    {
      return false;
    }
  return true;
}

bool
removing_trn_by_id_works ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  if (ce->n_transactions != 5)
    {
      return false;
    }
  casheph_remove_trn (ce, "b83f85a497dfb3f1d8db4c26489f57d9");
  if (ce->n_transactions != 4)
    {
      return false;
    }
  return (!casheph_get_transaction (ce, "b83f85a497dfb3f1d8db4c26489f57d9")
          && casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8")
          && casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0")
          && casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197")
          && casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457"));
}

bool
adding_simple_trn ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  casheph_account_t *root = ce->root;
  casheph_account_t *assets;
  assets = casheph_account_get_account_by_name (root, "Assets");
  casheph_account_t *cur_assets;
  cur_assets = casheph_account_get_account_by_name (assets, "Current Assets");
  casheph_account_t *checking;
  checking = casheph_account_get_account_by_name (cur_assets, "Checking Account");
  casheph_account_t *expenses;
  expenses = casheph_account_get_account_by_name (root, "Expenses");
  casheph_account_t *groceries;
  groceries = casheph_account_get_account_by_name (expenses, "Groceries");
  casheph_val_t *val = (casheph_val_t*)malloc (sizeof (casheph_val_t));
  val->n = 20453;
  val->d = 100;
  casheph_gdate_t *date = (casheph_gdate_t*)malloc (sizeof (casheph_gdate_t));
  date->year = 2013;
  date->month = 2;
  date->day = 22;
  casheph_transaction_t *trn;
  trn = casheph_add_simple_trn (ce, checking, groceries, date, val,
                                "Bought some groceries");
  if (trn == NULL)
    {
      return false;
    }
  if (ce->n_transactions != 6)
    {
      return false;
    }
  struct tm tm;
  tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
  tm.tm_mday = 22;
  tm.tm_mon = 1;
  tm.tm_year = 113;
  tm.tm_isdst = -1;
  time_t t = mktime (&tm);
  casheph_transaction_t *got_trn = casheph_get_transaction (ce, trn->id);
  if (got_trn == NULL
      || casheph_trn_value_for_act (got_trn, checking) == NULL
      || casheph_trn_value_for_act (got_trn, groceries) == NULL
      || casheph_trn_value_for_act (got_trn, checking)->n != -20453
      || casheph_trn_value_for_act (got_trn, checking)->d != 100
      || casheph_trn_value_for_act (got_trn, groceries)->n != 20453
      || casheph_trn_value_for_act (got_trn, groceries)->d != 100
      || strcmp (got_trn->desc, "Bought some groceries") != 0
      || got_trn->date_posted != t)
    {
      return false;
    }
  return true;
}

bool
get_account_by_id ()
{
  casheph_t *ce = casheph_open ("test.gnucash");
  casheph_account_t *act = casheph_get_account (ce, "5c40e1bcbf05128a9dd754c76e9fe959");
  if (act == NULL) return false;
  if (strcmp (act->name, "Checking Account") != 0) return false;
  return true;
}

#define CE_TEST(r, f, s) r = r && test (f, s)

int
main (int argc, char *argv[])
{
  bool res = true;
  CE_TEST (res, opening_gives_obj_with_accounts,
           "Opening gives object with accounts");
  CE_TEST (res, opening_gives_null_when_not_found,
           "Opening gives NULL when not found");
  CE_TEST (res, opening_gives_null_when_no_xml_dec,
           "Opening gives NULL when there is no XML declaration");
  CE_TEST (res, parsing_simple_tag,
           "Parsing a simple XML tag works");
  CE_TEST (res, parsing_simple_tags,
           "Parsing multiple simple XML tags in a row works");
  CE_TEST (res, parsing_attribute,
           "Parsing tag attributes (alone) works");
  CE_TEST (res, parsing_tag_with_attributes,
           "Parsing a tag with attributes works");
  CE_TEST (res, root_account_id,
           "The correct root account ID is detected [test.gnucash]");
  CE_TEST (res, top_accounts_have_correct_names,
           "The accounts under Root have the correct names [test.gnucash]");
  CE_TEST (res, assets_has_current_assets,
           "Assets has a child account called Current Assets [test.gnucash]");
  CE_TEST (res, has_five_transactions,
           "There are 5 transactions [test.gnucash]");
  CE_TEST (res, transaction_ids,
           "The transactions have the correct IDs [test.gnucash]");
  CE_TEST (res, transaction_descs,
           "The transactions have the correct descriptions [test.gnucash]");
  CE_TEST (res, transaction_date_posted,
           "The transactions have the correct posted dates [test.gnucash]");
  CE_TEST (res, transaction_date_posted_2,
           "The transactions have the correct posted dates [test2.gnucash]");
  CE_TEST (res, transaction_values_for_checking,
           "The transactions have the correct values for checking [test.gnucash]");
  CE_TEST (res, transaction_date_entered,
           "The transactions have the correct entered dates [test.gnucash]");
  CE_TEST (res, transaction_slots,
           "The transactions have the correct slots [test.gnucash]");
  CE_TEST (res, book_id_is_correct,
           "The book id is correct [test.gnucash]");
  CE_TEST (res, saving_produces_file_with_same_content,
           "Saving produces a file with the same content [test.gnucash]");
  CE_TEST (res, saving_produces_file_with_same_content_2,
           "Saving produces a file with the same content [test2.gnucash]");
  CE_TEST (res, some_have_zero_template_transactions,
           "Some have zero template transactions");
  CE_TEST (res, test3_has_four_template_transactions,
           "test3.gnucash has four template transactions");
  CE_TEST (res, test3_template_root_has_four_accounts,
           "test3.gnucash has a template root with four accounts in it");
  CE_TEST (res, test3_template_trn_splits_have_frame_slots_with_5_slots,
           "test3.gnucash template trn splits have frame slots with 5 slots");
  CE_TEST (res, saving_produces_file_with_same_content_3,
           "Saving produces a file with the same content [test3.gnucash]");
  CE_TEST (res, saving_produces_file_with_same_content_4,
           "Saving produces a file with the same content [test4.gnucash]");
  CE_TEST (res, removing_trn_by_id_works,
           "Removing transactions by ID works [test.gnucash]");
  CE_TEST (res, adding_simple_trn,
           "Adding a simple transaction (A->B) works [test.gnucash]");
  CE_TEST (res, get_account_by_id,
           "You can retrieve an account by ID [test.gnucash]");
  return res?0:1;
}
