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
  if (casheph_trn_value_for_act (t, checking) != 20000)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "75fe0a336df6675568885a8cd7c582a8");
  if (casheph_trn_value_for_act (t, checking) != -3214)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "26d5b26ad0b23fd822f2c63a6e1084e0");
  if (casheph_trn_value_for_act (t, checking) != -4823)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "b1bac36e34d568e6363a81f2f61af197");
  if (casheph_trn_value_for_act (t, checking) != 500279)
    {
      return false;
    }
  t = casheph_get_transaction (ce, "2205e761a5c5abbc66f34be4e212e457");
  if (casheph_trn_value_for_act (t, checking) != -312766)
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
  if (t->slots[0]->type != gdate)
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
  return res?0:1;
}
