/* tree.c -- helper functions to build and evaluate the expression tree.
   Copyright (C) 1990, 91, 92, 93, 94, 2000, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.
*/

#include "defs.h"
#include "../gnulib/lib/xalloc.h"

#include <assert.h>

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif
#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
/* See locate.c for explanation as to why not use (String) */
# define N_(String) String
#endif



static struct predicate *predicates = NULL;
static struct predicate *eval_tree  = NULL;



static struct predicate *scan_rest PARAMS((struct predicate **input,
				       struct predicate *head,
				       short int prev_prec));
static void merge_pred PARAMS((struct predicate *beg_list, struct predicate *end_list, struct predicate **last_p));
static struct predicate *set_new_parent PARAMS((struct predicate *curr, enum predicate_precedence high_prec, struct predicate **prevp));

/* Return a pointer to a tree that represents the
   expression prior to non-unary operator *INPUT.
   Set *INPUT to point at the next input predicate node.

   Only accepts the following:
   
   <primary>
   expression		[operators of higher precedence]
   <uni_op><primary>
   (arbitrary expression)
   <uni_op>(arbitrary expression)
   
   In other words, you can not start out with a bi_op or close_paren.

   If the following operator (if any) is of a higher precedence than
   PREV_PREC, the expression just nabbed is part of a following
   expression, which really is the expression that should be handed to
   our caller, so get_expr recurses. */

struct predicate *
get_expr (struct predicate **input,
	  short int prev_prec,
	  const struct predicate* prev_pred)
{
  struct predicate *next = NULL;
  struct predicate *this_pred = (*input);

  if (*input == NULL)
    error (1, 0, _("invalid expression"));
  
  switch ((*input)->p_type)
    {
    case NO_TYPE:
      error (1, 0, _("invalid expression"));
      break;

    case BI_OP:
      /* e.g. "find . -a" */
      error (1, 0, _("invalid expression; you have used a binary operator '%s' with nothing before it."), this_pred->p_name);
      break;

    case CLOSE_PAREN:
      if ((UNI_OP == prev_pred->p_type
	  || BI_OP == prev_pred->p_type)
	  && !this_pred->artificial)
	{
	  /* e.g. "find \( -not \)" or "find \( -true -a \" */
	  error(1, 0, _("expected an expression between '%s' and ')'"),
		prev_pred->p_name);
	}
      else if ( (*input)->artificial )
	{
	  /* We have reached the end of the user-supplied predicates
	   * unexpectedly. 
	   */
	  /* e.g. "find . -true -a" */
	  error (1, 0, _("expected an expression after '%s'"), prev_pred->p_name);
	}
      else
	{
	  error (1, 0, _("invalid expression; you have too many ')'"));
	}
      break;

    case PRIMARY_TYPE:
      next = *input;
      *input = (*input)->pred_next;
      break;

    case UNI_OP:
      next = *input;
      *input = (*input)->pred_next;
      next->pred_right = get_expr (input, NEGATE_PREC, next);
      break;

    case OPEN_PAREN:
      if ( (NULL == (*input)->pred_next) || (*input)->pred_next->artificial )
	{
	  /* user typed something like "find . (", and so the ) we are
	   * looking at is from the artificial "( ) -print" that we
	   * add.
	   */
	  error (1, 0, _("invalid expression; expected to find a ')' but didn't see one.  Perhaps you need an extra predicate after '%s'"), this_pred->p_name);
	}
      prev_pred = (*input);
      *input = (*input)->pred_next;
      if ( (*input)->p_type == CLOSE_PAREN )
	{
	  error (1, 0, _("invalid expression; empty parentheses are not allowed."));
	}
      next = get_expr (input, NO_PREC, prev_pred);
      if ((*input == NULL)
	  || ((*input)->p_type != CLOSE_PAREN))
	error (1, 0, _("invalid expression; I was expecting to find a ')' somewhere but did not see one."));
      *input = (*input)->pred_next;	/* move over close */
      break;
      
    default:
      error (1, 0, _("oops -- invalid expression type!"));
      break;
    }

  /* We now have the first expression and are positioned to check
     out the next operator.  If NULL, all done.  Otherwise, if
     PREV_PREC < the current node precedence, we must continue;
     the expression we just nabbed is more tightly bound to the
     following expression than to the previous one. */
  if (*input == NULL)
    return (next);
  if ((int) (*input)->p_prec > (int) prev_prec)
    {
      next = scan_rest (input, next, prev_prec);
      if (next == NULL)
	error (1, 0, _("invalid expression"));
    }
  return (next);
}

/* Scan across the remainder of a predicate input list starting
   at *INPUT, building the rest of the expression tree to return.
   Stop at the first close parenthesis or the end of the input list.
   Assumes that get_expr has been called to nab the first element
   of the expression tree.
   
   *INPUT points to the current input predicate list element.
   It is updated as we move along the list to point to the
   terminating input element.
   HEAD points to the predicate element that was obtained
   by the call to get_expr.
   PREV_PREC is the precedence of the previous predicate element. */

static struct predicate *
scan_rest (struct predicate **input,
	   struct predicate *head,
	   short int prev_prec)
{
  struct predicate *tree;	/* The new tree we are building. */

  if ((*input == NULL) || ((*input)->p_type == CLOSE_PAREN))
    return (NULL);
  tree = head;
  while ((*input != NULL) && ((int) (*input)->p_prec > (int) prev_prec))
    {
      switch ((*input)->p_type)
	{
	case NO_TYPE:
	case PRIMARY_TYPE:
	case UNI_OP:
	case OPEN_PAREN:
	  /* I'm not sure how we get here, so it is not obvious what
	   * sort of mistakes might give rise to this condition.
	   */
	  error (1, 0, _("invalid expression"));
	  break;

	case BI_OP:
	  {
	    struct predicate *prev = (*input);
	    (*input)->pred_left = tree;
	    tree = *input;
	    *input = (*input)->pred_next;
	    tree->pred_right = get_expr (input, tree->p_prec, prev);
	    break;
	  }

	case CLOSE_PAREN:
	  return tree;

	default:
	  error (1, 0,
		 _("oops -- invalid expression type (%d)!"),
		 (int)(*input)->p_type);
	  break;
	}
    }
  return tree;
}

/* Optimize the ordering of the predicates in the tree.  Rearrange
   them to minimize work.  Strategies:
   * Evaluate predicates that don't need inode information first;
     the predicates are divided into 1 or more groups separated by
     predicates (if any) which have "side effects", such as printing.
     The grouping implements the partial ordering on predicates which
     those with side effects impose.

   * Place -name, -iname, -path, -ipath, -regex and -iregex at the front
     of a group, with -name, -iname, -path and -ipath ahead of
     -regex and -iregex.  Predicates which are moved to the front
     of a group by definition do not have side effects.  Both
     -regex and -iregex both use pred_regex.

     This routine "normalizes" the predicate tree by ensuring that
     all expression predicates have AND (or OR or COMMA) parent nodes
     which are linked along the left edge of the expression tree.
     This makes manipulation of subtrees easier.  

     EVAL_TREEP points to the root pointer of the predicate tree
     to be rearranged.  opt_expr may return a new root pointer there.
     Return true if the tree contains side effects, false if not. */

static boolean
opt_expr (struct predicate **eval_treep)
{
  /* List of -name and -path predicates to move. */
  struct predicate *name_list = NULL;
  struct predicate *end_name_list = NULL;
  /* List of -regex predicates to move. */
  struct predicate *regex_list = NULL;
  struct predicate *end_regex_list = NULL;
  struct predicate *curr;
  struct predicate **prevp;	/* Address of `curr' node. */
  struct predicate **last_sidep; /* Last predicate with side effects. */
  PRED_FUNC pred_func;
  enum predicate_type p_type;
  boolean has_side_effects = false; /* Return value. */
  enum predicate_precedence prev_prec, /* precedence of last BI_OP in branch */
			    biop_prec; /* topmost BI_OP precedence in branch */


  if (eval_treep == NULL || *eval_treep == NULL)
    return (false);

  /* Set up to normalize tree as a left-linked list of ANDs or ORs.
     Set `curr' to the leftmost node, `prevp' to its address, and
     `pred_func' to the predicate type of its parent. */
  prevp = eval_treep;
  prev_prec = AND_PREC;
  curr = *prevp;
  while (curr->pred_left != NULL)
    {
      prevp = &curr->pred_left;
      prev_prec = curr->p_prec;	/* must be a BI_OP */
      curr = curr->pred_left;
    }

  /* Link in the appropriate BI_OP for the last expression, if needed. */
  if (curr->p_type != BI_OP)
    set_new_parent (curr, prev_prec, prevp);
  
#ifdef DEBUG
  /* Normalized tree. */
  fprintf (stderr, "Normalized Eval Tree:\n");
  print_tree (stderr, *eval_treep, 0);
#endif

  /* Rearrange the predicates. */
  prevp = eval_treep;
  biop_prec = NO_PREC; /* not COMMA_PREC */
  if ((*prevp) && (*prevp)->p_type == BI_OP)
    biop_prec = (*prevp)->p_prec;
  while ((curr = *prevp) != NULL)
    {
      /* If there is a BI_OP of different precedence from the first
	 in the pred_left chain, create a new parent of the
	 original precedence, link the new parent to the left of the
	 previous and link CURR to the right of the new parent. 
	 This preserves the precedence of expressions in the tree
	 in case we rearrange them. */
      if (curr->p_type == BI_OP)
	{
          if (curr->p_prec != biop_prec)
	    curr = set_new_parent(curr, biop_prec, prevp);
	}
	  
      /* See which predicate type we have. */
      p_type = curr->pred_right->p_type;
      pred_func = curr->pred_right->pred_func;

      switch (p_type)
	{
	case NO_TYPE:
	case PRIMARY_TYPE:
	  /* Don't rearrange the arguments of the comma operator, it is
	     not commutative.  */
	  if (biop_prec == COMMA_PREC)
	    break;

	  /* If it's one of our special primaries, move it to the
	     front of the list for that primary. */
	  if (pred_func == pred_name || pred_func == pred_path ||
	      pred_func == pred_iname || pred_func == pred_ipath)
	    {
	      *prevp = curr->pred_left;
	      curr->pred_left = name_list;
	      name_list = curr;

	      if (end_name_list == NULL)
		end_name_list = curr;

	      continue;
	    }

	  if (pred_func == pred_regex)
	    {
	      *prevp = curr->pred_left;
	      curr->pred_left = regex_list;
	      regex_list = curr;

	      if (end_regex_list == NULL)
		end_regex_list = curr;

	      continue;
	    }

	  break;

	case UNI_OP:
	  /* For NOT, check the expression trees below the NOT. */
	  curr->pred_right->side_effects
	    = opt_expr (&curr->pred_right->pred_right);
	  break;

	case BI_OP:
	  /* For nested AND or OR, recurse (AND/OR form layers on the left of
	     the tree), and continue scanning this level of AND or OR. */
	  curr->pred_right->side_effects = opt_expr (&curr->pred_right);
	  break;

	  /* At this point, get_expr and scan_rest have already removed
	     all of the user's parentheses. */

	default:
	  error (1, 0, _("oops -- invalid expression type!"));
	  break;
	}

      if (curr->pred_right->side_effects == true)
	{
	  last_sidep = prevp;

	  /* Incorporate lists and reset list pointers for this group.  */
	  if (name_list != NULL)
	    {
	      merge_pred (name_list, end_name_list, last_sidep);
	      name_list = end_name_list = NULL;
	    }

	  if (regex_list != NULL)
	    {
	      merge_pred (regex_list, end_regex_list, last_sidep);
	      regex_list = end_regex_list = NULL;
	    }

	  has_side_effects = true;
	}

      prevp = &curr->pred_left;
    }

  /* Do final list merges. */
  last_sidep = prevp;
  if (name_list != NULL)
    merge_pred (name_list, end_name_list, last_sidep);
  if (regex_list != NULL)
    merge_pred (regex_list, end_regex_list, last_sidep);

  return (has_side_effects);
}

/* Link in a new parent BI_OP node for CURR, at *PREVP, with precedence
   HIGH_PREC. */

static struct predicate *
set_new_parent (struct predicate *curr, enum predicate_precedence high_prec, struct predicate **prevp)
{
  struct predicate *new_parent;

  new_parent = (struct predicate *) xmalloc (sizeof (struct predicate));
  new_parent->p_type = BI_OP;
  new_parent->p_prec = high_prec;
  new_parent->need_stat = false;
  new_parent->need_type = false;

  switch (high_prec)
    {
    case COMMA_PREC:
      new_parent->pred_func = pred_comma;
      break;
    case OR_PREC:
      new_parent->pred_func = pred_or;
      break;
    case AND_PREC:
      new_parent->pred_func = pred_and;
      break;
    default:
      ;				/* empty */
    }
  
  new_parent->side_effects = false;
  new_parent->no_default_print = false;
  new_parent->args.str = NULL;
  new_parent->pred_next = NULL;

  /* Link in new_parent.
     Pushes rest of left branch down 1 level to new_parent->pred_right. */
  new_parent->pred_left = NULL;
  new_parent->pred_right = curr;
  *prevp = new_parent;

#ifdef	DEBUG
  new_parent->p_name = (char *) find_pred_name (new_parent->pred_func);
#endif /* DEBUG */

  return (new_parent);
}

/* Merge the predicate list that starts at BEG_LIST and ends at END_LIST
   into the tree at LAST_P. */

static void
merge_pred (struct predicate *beg_list, struct predicate *end_list, struct predicate **last_p)
{
  end_list->pred_left = *last_p;
  *last_p = beg_list;
}

/* Find the first node in expression tree TREE that requires
   a stat call and mark the operator above it as needing a stat
   before calling the node.   Since the expression precedences 
   are represented in the tree, some preds that need stat may not
   get executed (because the expression value is determined earlier.)
   So every expression needing stat must be marked as such, not just
   the earliest, to be sure to obtain the stat.  This still guarantees 
   that a stat is made as late as possible.  Return true if the top node 
   in TREE requires a stat, false if not. */

static boolean
mark_stat (struct predicate *tree)
{
  /* The tree is executed in-order, so walk this way (apologies to Aerosmith)
     to find the first predicate for which the stat is needed. */
  switch (tree->p_type)
    {
    case NO_TYPE:
    case PRIMARY_TYPE:
      return tree->need_stat;

    case UNI_OP:
      if (mark_stat (tree->pred_right))
	tree->need_stat = true;
      return (false);

    case BI_OP:
      /* ANDs and ORs are linked along ->left ending in NULL. */
      if (tree->pred_left != NULL)
	mark_stat (tree->pred_left);

      if (mark_stat (tree->pred_right))
	tree->need_stat = true;

      return (false);

    default:
      error (1, 0, _("oops -- invalid expression type in mark_stat!"));
      return (false);
    }
}

/* Find the first node in expression tree TREE that we will
   need to know the file type, if any.   Operates in the same 
   was as mark_stat().
*/
static boolean
mark_type (struct predicate *tree)
{
  /* The tree is executed in-order, so walk this way (apologies to Aerosmith)
     to find the first predicate for which the type information is needed. */
  switch (tree->p_type)
    {
    case NO_TYPE:
    case PRIMARY_TYPE:
      return tree->need_type;

    case UNI_OP:
      if (mark_type (tree->pred_right))
	tree->need_type = true;
      return false;

    case BI_OP:
      /* ANDs and ORs are linked along ->left ending in NULL. */
      if (tree->pred_left != NULL)
	mark_type (tree->pred_left);

      if (mark_type (tree->pred_right))
	tree->need_type = true;

      return false;

    default:
      error (1, 0, _("oops -- invalid expression type in mark_type!"));
      return (false);
    }
}


struct predicate*
get_eval_tree(void)
{
  return eval_tree;
}

struct predicate*
build_expression_tree(int argc, char *argv[], int end_of_leading_options)
{
  const struct parser_table *parse_entry; /* Pointer to the parsing table entry for this expression. */
  char *predicate_name;		/* Name of predicate being parsed. */
  struct predicate *cur_pred;
  const struct parser_table *entry_close, *entry_print, *entry_open;
  int i;

  predicates = NULL;
  
  /* Find where in ARGV the predicates begin by skipping the list of start points. */
  for (i = end_of_leading_options; i < argc && !looks_like_expression(argv[i], true); i++)
    {
      /* fprintf(stderr, "Looks like %s is not a predicate\n", argv[i]); */
      /* Do nothing. */ ;
    }
  
  /* XXX: beginning of bit we need factor out of both find.c and ftsfind.c */
  /* Enclose the expression in `( ... )' so a default -print will
     apply to the whole expression. */
  entry_open  = find_parser("(");
  entry_close = find_parser(")");
  entry_print = find_parser("print");
  assert(entry_open  != NULL);
  assert(entry_close != NULL);
  assert(entry_print != NULL);
  
  parse_open (entry_open, argv, &argc);
  predicates->artificial = true;
  parse_begin_user_args(argv, argc, last_pred, predicates);
  pred_sanity_check(last_pred);
  
  /* Build the input order list. */
  while (i < argc )
    {
      if (!looks_like_expression(argv[i], false))
	{
	  error (0, 0, _("paths must precede expression: %s"), argv[i]);
	  usage(NULL);
	}

      predicate_name = argv[i];
      parse_entry = find_parser (predicate_name);
      if (parse_entry == NULL)
	{
	  /* Command line option not recognized */
	  error (1, 0, _("invalid predicate `%s'"), predicate_name);
	}
      
      i++;
      if (!(*(parse_entry->parser_func)) (parse_entry, argv, &i))
	{
	  if (argv[i] == NULL)
	    /* Command line option requires an argument */
	    error (1, 0, _("missing argument to `%s'"), predicate_name);
	  else
	    error (1, 0, _("invalid argument `%s' to `%s'"),
		   argv[i], predicate_name);
	}
      else
	{
	  last_pred->p_name = predicate_name;
	}
      pred_sanity_check(last_pred);
      pred_sanity_check(predicates); /* XXX: expensive */
    }
  parse_end_user_args(argv, argc, last_pred, predicates);
  if (predicates->pred_next == NULL)
    {
      /* No predicates that do something other than set a global variable
	 were given; remove the unneeded initial `(' and add `-print'. */
      cur_pred = predicates;
      predicates = last_pred = predicates->pred_next;
      free ((char *) cur_pred);
      parse_print (entry_print, argv, &argc);
      pred_sanity_check(last_pred); 
      pred_sanity_check(predicates); /* XXX: expensive */
    }
  else if (!default_prints (predicates->pred_next))
    {
      /* One or more predicates that produce output were given;
	 remove the unneeded initial `('. */
      cur_pred = predicates;
      predicates = predicates->pred_next;
      pred_sanity_check(predicates); /* XXX: expensive */
      free ((char *) cur_pred);
    }
  else
    {
      /* `( user-supplied-expression ) -print'. */
      parse_close (entry_close, argv, &argc);
      last_pred->artificial = true;
      pred_sanity_check(last_pred);
      parse_print (entry_print, argv, &argc);
      last_pred->artificial = true;
      pred_sanity_check(last_pred);
      pred_sanity_check(predicates); /* XXX: expensive */
    }

#ifdef	DEBUG
  fprintf (stderr, "Predicate List:\n");
  print_list (stderr, predicates);
#endif /* DEBUG */

  /* do a sanity check */
  pred_sanity_check(predicates);
  
  /* Done parsing the predicates.  Build the evaluation tree. */
  cur_pred = predicates;
  eval_tree = get_expr (&cur_pred, NO_PREC, NULL);

  /* Check if we have any left-over predicates (this fixes
   * Debian bug #185202).
   */
  if (cur_pred != NULL)
    {
      /* cur_pred->p_name is often NULL here */
      if (cur_pred->pred_func == pred_close)
	{
	  /* e.g. "find \( -true \) \)" */
	  error (1, 0, _("you have too many ')'"), cur_pred->p_name);
	}
      else
	{
	  if (cur_pred->p_name)
	    error (1, 0, _("unexpected extra predicate '%s'"), cur_pred->p_name);
	  else
	    error (1, 0, _("unexpected extra predicate"));
	}
    }
  
#ifdef	DEBUG
  fprintf (stderr, "Eval Tree:\n");
  print_tree (stderr, eval_tree, 0);
#endif /* DEBUG */

  /* Rearrange the eval tree in optimal-predicate order. */
  opt_expr (&eval_tree);

  /* Determine the point, if any, at which to stat the file. */
  mark_stat (eval_tree);
  /* Determine the point, if any, at which to determine file type. */
  mark_type (eval_tree);

#ifdef DEBUG
  fprintf (stderr, "Optimized Eval Tree:\n");
  print_tree (stderr, eval_tree, 0);
  fprintf (stderr, "Optimized command line:\n");
  print_optlist(stderr, eval_tree);
  fprintf(stderr, "\n");
#endif /* DEBUG */


  return eval_tree;
}

/* Return a pointer to a new predicate structure, which has been
   linked in as the last one in the predicates list.

   Set `predicates' to point to the start of the predicates list.
   Set `last_pred' to point to the new last predicate in the list.
   
   Set all cells in the new structure to the default values. */

struct predicate *
get_new_pred (const struct parser_table *entry)
{
  register struct predicate *new_pred;
  (void) entry;

  /* Options should not be turned into predicates. */
  assert(entry->type != ARG_OPTION);
  assert(entry->type != ARG_POSITIONAL_OPTION);
  
  if (predicates == NULL)
    {
      predicates = (struct predicate *)
	xmalloc (sizeof (struct predicate));
      last_pred = predicates;
    }
  else
    {
      new_pred = (struct predicate *) xmalloc (sizeof (struct predicate));
      last_pred->pred_next = new_pred;
      last_pred = new_pred;
    }
  last_pred->parser_entry = entry;
  last_pred->pred_func = NULL;
#ifdef	DEBUG
  last_pred->p_name = NULL;
#endif	/* DEBUG */
  last_pred->p_type = NO_TYPE;
  last_pred->p_prec = NO_PREC;
  last_pred->side_effects = false;
  last_pred->no_default_print = false;
  last_pred->need_stat = true;
  last_pred->need_type = true;
  last_pred->args.str = NULL;
  last_pred->pred_next = NULL;
  last_pred->pred_left = NULL;
  last_pred->pred_right = NULL;
  last_pred->literal_control_chars = options.literal_control_chars;
  last_pred->artificial = false;
  return last_pred;
}
