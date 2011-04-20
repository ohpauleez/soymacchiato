/*
 * Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "compiler/compileLog.hpp"
#include "memory/allocation.inline.hpp"
#include "opto/addnode.hpp"
#include "opto/callnode.hpp"
#include "opto/connode.hpp"
#include "opto/divnode.hpp"
#include "opto/loopnode.hpp"
#include "opto/mulnode.hpp"
#include "opto/rootnode.hpp"
#include "opto/runtime.hpp"
#include "opto/subnode.hpp"

//------------------------------is_loop_exit-----------------------------------
// Given an IfNode, return the loop-exiting projection or NULL if both
// arms remain in the loop.
Node *IdealLoopTree::is_loop_exit(Node *iff) const {
  if( iff->outcnt() != 2 ) return NULL; // Ignore partially dead tests
  PhaseIdealLoop *phase = _phase;
  // Test is an IfNode, has 2 projections.  If BOTH are in the loop
  // we need loop unswitching instead of peeling.
  if( !is_member(phase->get_loop( iff->raw_out(0) )) )
    return iff->raw_out(0);
  if( !is_member(phase->get_loop( iff->raw_out(1) )) )
    return iff->raw_out(1);
  return NULL;
}


//=============================================================================


//------------------------------record_for_igvn----------------------------
// Put loop body on igvn work list
void IdealLoopTree::record_for_igvn() {
  for( uint i = 0; i < _body.size(); i++ ) {
    Node *n = _body.at(i);
    _phase->_igvn._worklist.push(n);
  }
}

//------------------------------compute_profile_trip_cnt----------------------------
// Compute loop trip count from profile data as
//    (backedge_count + loop_exit_count) / loop_exit_count
void IdealLoopTree::compute_profile_trip_cnt( PhaseIdealLoop *phase ) {
  if (!_head->is_CountedLoop()) {
    return;
  }
  CountedLoopNode* head = _head->as_CountedLoop();
  if (head->profile_trip_cnt() != COUNT_UNKNOWN) {
    return; // Already computed
  }
  float trip_cnt = (float)max_jint; // default is big

  Node* back = head->in(LoopNode::LoopBackControl);
  while (back != head) {
    if ((back->Opcode() == Op_IfTrue || back->Opcode() == Op_IfFalse) &&
        back->in(0) &&
        back->in(0)->is_If() &&
        back->in(0)->as_If()->_fcnt != COUNT_UNKNOWN &&
        back->in(0)->as_If()->_prob != PROB_UNKNOWN) {
      break;
    }
    back = phase->idom(back);
  }
  if (back != head) {
    assert((back->Opcode() == Op_IfTrue || back->Opcode() == Op_IfFalse) &&
           back->in(0), "if-projection exists");
    IfNode* back_if = back->in(0)->as_If();
    float loop_back_cnt = back_if->_fcnt * back_if->_prob;

    // Now compute a loop exit count
    float loop_exit_cnt = 0.0f;
    for( uint i = 0; i < _body.size(); i++ ) {
      Node *n = _body[i];
      if( n->is_If() ) {
        IfNode *iff = n->as_If();
        if( iff->_fcnt != COUNT_UNKNOWN && iff->_prob != PROB_UNKNOWN ) {
          Node *exit = is_loop_exit(iff);
          if( exit ) {
            float exit_prob = iff->_prob;
            if (exit->Opcode() == Op_IfFalse) exit_prob = 1.0 - exit_prob;
            if (exit_prob > PROB_MIN) {
              float exit_cnt = iff->_fcnt * exit_prob;
              loop_exit_cnt += exit_cnt;
            }
          }
        }
      }
    }
    if (loop_exit_cnt > 0.0f) {
      trip_cnt = (loop_back_cnt + loop_exit_cnt) / loop_exit_cnt;
    } else {
      // No exit count so use
      trip_cnt = loop_back_cnt;
    }
  }
#ifndef PRODUCT
  if (TraceProfileTripCount) {
    tty->print_cr("compute_profile_trip_cnt  lp: %d cnt: %f\n", head->_idx, trip_cnt);
  }
#endif
  head->set_profile_trip_cnt(trip_cnt);
}

//---------------------is_invariant_addition-----------------------------
// Return nonzero index of invariant operand for an Add or Sub
// of (nonconstant) invariant and variant values. Helper for reassociate_invariants.
int IdealLoopTree::is_invariant_addition(Node* n, PhaseIdealLoop *phase) {
  int op = n->Opcode();
  if (op == Op_AddI || op == Op_SubI) {
    bool in1_invar = this->is_invariant(n->in(1));
    bool in2_invar = this->is_invariant(n->in(2));
    if (in1_invar && !in2_invar) return 1;
    if (!in1_invar && in2_invar) return 2;
  }
  return 0;
}

//---------------------reassociate_add_sub-----------------------------
// Reassociate invariant add and subtract expressions:
//
// inv1 + (x + inv2)  =>  ( inv1 + inv2) + x
// (x + inv2) + inv1  =>  ( inv1 + inv2) + x
// inv1 + (x - inv2)  =>  ( inv1 - inv2) + x
// inv1 - (inv2 - x)  =>  ( inv1 - inv2) + x
// (x + inv2) - inv1  =>  (-inv1 + inv2) + x
// (x - inv2) + inv1  =>  ( inv1 - inv2) + x
// (x - inv2) - inv1  =>  (-inv1 - inv2) + x
// inv1 + (inv2 - x)  =>  ( inv1 + inv2) - x
// inv1 - (x - inv2)  =>  ( inv1 + inv2) - x
// (inv2 - x) + inv1  =>  ( inv1 + inv2) - x
// (inv2 - x) - inv1  =>  (-inv1 + inv2) - x
// inv1 - (x + inv2)  =>  ( inv1 - inv2) - x
//
Node* IdealLoopTree::reassociate_add_sub(Node* n1, PhaseIdealLoop *phase) {
  if (!n1->is_Add() && !n1->is_Sub() || n1->outcnt() == 0) return NULL;
  if (is_invariant(n1)) return NULL;
  int inv1_idx = is_invariant_addition(n1, phase);
  if (!inv1_idx) return NULL;
  // Don't mess with add of constant (igvn moves them to expression tree root.)
  if (n1->is_Add() && n1->in(2)->is_Con()) return NULL;
  Node* inv1 = n1->in(inv1_idx);
  Node* n2 = n1->in(3 - inv1_idx);
  int inv2_idx = is_invariant_addition(n2, phase);
  if (!inv2_idx) return NULL;
  Node* x    = n2->in(3 - inv2_idx);
  Node* inv2 = n2->in(inv2_idx);

  bool neg_x    = n2->is_Sub() && inv2_idx == 1;
  bool neg_inv2 = n2->is_Sub() && inv2_idx == 2;
  bool neg_inv1 = n1->is_Sub() && inv1_idx == 2;
  if (n1->is_Sub() && inv1_idx == 1) {
    neg_x    = !neg_x;
    neg_inv2 = !neg_inv2;
  }
  Node* inv1_c = phase->get_ctrl(inv1);
  Node* inv2_c = phase->get_ctrl(inv2);
  Node* n_inv1;
  if (neg_inv1) {
    Node *zero = phase->_igvn.intcon(0);
    phase->set_ctrl(zero, phase->C->root());
    n_inv1 = new (phase->C, 3) SubINode(zero, inv1);
    phase->register_new_node(n_inv1, inv1_c);
  } else {
    n_inv1 = inv1;
  }
  Node* inv;
  if (neg_inv2) {
    inv = new (phase->C, 3) SubINode(n_inv1, inv2);
  } else {
    inv = new (phase->C, 3) AddINode(n_inv1, inv2);
  }
  phase->register_new_node(inv, phase->get_early_ctrl(inv));

  Node* addx;
  if (neg_x) {
    addx = new (phase->C, 3) SubINode(inv, x);
  } else {
    addx = new (phase->C, 3) AddINode(x, inv);
  }
  phase->register_new_node(addx, phase->get_ctrl(x));
  phase->_igvn.replace_node(n1, addx);
  assert(phase->get_loop(phase->get_ctrl(n1)) == this, "");
  _body.yank(n1);
  return addx;
}

//---------------------reassociate_invariants-----------------------------
// Reassociate invariant expressions:
void IdealLoopTree::reassociate_invariants(PhaseIdealLoop *phase) {
  for (int i = _body.size() - 1; i >= 0; i--) {
    Node *n = _body.at(i);
    for (int j = 0; j < 5; j++) {
      Node* nn = reassociate_add_sub(n, phase);
      if (nn == NULL) break;
      n = nn; // again
    };
  }
}

//------------------------------policy_peeling---------------------------------
// Return TRUE or FALSE if the loop should be peeled or not.  Peel if we can
// make some loop-invariant test (usually a null-check) happen before the loop.
bool IdealLoopTree::policy_peeling( PhaseIdealLoop *phase ) const {
  Node *test = ((IdealLoopTree*)this)->tail();
  int  body_size = ((IdealLoopTree*)this)->_body.size();
  int  uniq      = phase->C->unique();
  // Peeling does loop cloning which can result in O(N^2) node construction
  if( body_size > 255 /* Prevent overflow for large body_size */
      || (body_size * body_size + uniq > MaxNodeLimit) ) {
    return false;           // too large to safely clone
  }
  while( test != _head ) {      // Scan till run off top of loop
    if( test->is_If() ) {       // Test?
      Node *ctrl = phase->get_ctrl(test->in(1));
      if (ctrl->is_top())
        return false;           // Found dead test on live IF?  No peeling!
      // Standard IF only has one input value to check for loop invariance
      assert( test->Opcode() == Op_If || test->Opcode() == Op_CountedLoopEnd, "Check this code when new subtype is added");
      // Condition is not a member of this loop?
      if( !is_member(phase->get_loop(ctrl)) &&
          is_loop_exit(test) )
        return true;            // Found reason to peel!
    }
    // Walk up dominators to loop _head looking for test which is
    // executed on every path thru loop.
    test = phase->idom(test);
  }
  return false;
}

//------------------------------peeled_dom_test_elim---------------------------
// If we got the effect of peeling, either by actually peeling or by making
// a pre-loop which must execute at least once, we can remove all
// loop-invariant dominated tests in the main body.
void PhaseIdealLoop::peeled_dom_test_elim( IdealLoopTree *loop, Node_List &old_new ) {
  bool progress = true;
  while( progress ) {
    progress = false;           // Reset for next iteration
    Node *prev = loop->_head->in(LoopNode::LoopBackControl);//loop->tail();
    Node *test = prev->in(0);
    while( test != loop->_head ) { // Scan till run off top of loop

      int p_op = prev->Opcode();
      if( (p_op == Op_IfFalse || p_op == Op_IfTrue) &&
          test->is_If() &&      // Test?
          !test->in(1)->is_Con() && // And not already obvious?
          // Condition is not a member of this loop?
          !loop->is_member(get_loop(get_ctrl(test->in(1))))){
        // Walk loop body looking for instances of this test
        for( uint i = 0; i < loop->_body.size(); i++ ) {
          Node *n = loop->_body.at(i);
          if( n->is_If() && n->in(1) == test->in(1) /*&& n != loop->tail()->in(0)*/ ) {
            // IfNode was dominated by version in peeled loop body
            progress = true;
            dominated_by( old_new[prev->_idx], n );
          }
        }
      }
      prev = test;
      test = idom(test);
    } // End of scan tests in loop

  } // End of while( progress )
}

//------------------------------do_peeling-------------------------------------
// Peel the first iteration of the given loop.
// Step 1: Clone the loop body.  The clone becomes the peeled iteration.
//         The pre-loop illegally has 2 control users (old & new loops).
// Step 2: Make the old-loop fall-in edges point to the peeled iteration.
//         Do this by making the old-loop fall-in edges act as if they came
//         around the loopback from the prior iteration (follow the old-loop
//         backedges) and then map to the new peeled iteration.  This leaves
//         the pre-loop with only 1 user (the new peeled iteration), but the
//         peeled-loop backedge has 2 users.
// Step 3: Cut the backedge on the clone (so its not a loop) and remove the
//         extra backedge user.
void PhaseIdealLoop::do_peeling( IdealLoopTree *loop, Node_List &old_new ) {

  C->set_major_progress();
  // Peeling a 'main' loop in a pre/main/post situation obfuscates the
  // 'pre' loop from the main and the 'pre' can no longer have it's
  // iterations adjusted.  Therefore, we need to declare this loop as
  // no longer a 'main' loop; it will need new pre and post loops before
  // we can do further RCE.
#ifndef PRODUCT
  if (TraceLoopOpts) {
    tty->print("Peel         ");
    loop->dump_head();
  }
#endif
  Node *h = loop->_head;
  if (h->is_CountedLoop()) {
    CountedLoopNode *cl = h->as_CountedLoop();
    assert(cl->trip_count() > 0, "peeling a fully unrolled loop");
    cl->set_trip_count(cl->trip_count() - 1);
    if (cl->is_main_loop()) {
      cl->set_normal_loop();
#ifndef PRODUCT
      if (PrintOpto && VerifyLoopOptimizations) {
        tty->print("Peeling a 'main' loop; resetting to 'normal' ");
        loop->dump_head();
      }
#endif
    }
  }

  // Step 1: Clone the loop body.  The clone becomes the peeled iteration.
  //         The pre-loop illegally has 2 control users (old & new loops).
  clone_loop( loop, old_new, dom_depth(loop->_head) );


  // Step 2: Make the old-loop fall-in edges point to the peeled iteration.
  //         Do this by making the old-loop fall-in edges act as if they came
  //         around the loopback from the prior iteration (follow the old-loop
  //         backedges) and then map to the new peeled iteration.  This leaves
  //         the pre-loop with only 1 user (the new peeled iteration), but the
  //         peeled-loop backedge has 2 users.
  for (DUIterator_Fast jmax, j = loop->_head->fast_outs(jmax); j < jmax; j++) {
    Node* old = loop->_head->fast_out(j);
    if( old->in(0) == loop->_head && old->req() == 3 &&
        (old->is_Loop() || old->is_Phi()) ) {
      Node *new_exit_value = old_new[old->in(LoopNode::LoopBackControl)->_idx];
      if( !new_exit_value )     // Backedge value is ALSO loop invariant?
        // Then loop body backedge value remains the same.
        new_exit_value = old->in(LoopNode::LoopBackControl);
      _igvn.hash_delete(old);
      old->set_req(LoopNode::EntryControl, new_exit_value);
    }
  }


  // Step 3: Cut the backedge on the clone (so its not a loop) and remove the
  //         extra backedge user.
  Node *nnn = old_new[loop->_head->_idx];
  _igvn.hash_delete(nnn);
  nnn->set_req(LoopNode::LoopBackControl, C->top());
  for (DUIterator_Fast j2max, j2 = nnn->fast_outs(j2max); j2 < j2max; j2++) {
    Node* use = nnn->fast_out(j2);
    if( use->in(0) == nnn && use->req() == 3 && use->is_Phi() ) {
      _igvn.hash_delete(use);
      use->set_req(LoopNode::LoopBackControl, C->top());
    }
  }


  // Step 4: Correct dom-depth info.  Set to loop-head depth.
  int dd = dom_depth(loop->_head);
  set_idom(loop->_head, loop->_head->in(1), dd);
  for (uint j3 = 0; j3 < loop->_body.size(); j3++) {
    Node *old = loop->_body.at(j3);
    Node *nnn = old_new[old->_idx];
    if (!has_ctrl(nnn))
      set_idom(nnn, idom(nnn), dd-1);
    // While we're at it, remove any SafePoints from the peeled code
    if( old->Opcode() == Op_SafePoint ) {
      Node *nnn = old_new[old->_idx];
      lazy_replace(nnn,nnn->in(TypeFunc::Control));
    }
  }

  // Now force out all loop-invariant dominating tests.  The optimizer
  // finds some, but we _know_ they are all useless.
  peeled_dom_test_elim(loop,old_new);

  loop->record_for_igvn();
}

//------------------------------policy_maximally_unroll------------------------
// Return exact loop trip count, or 0 if not maximally unrolling
bool IdealLoopTree::policy_maximally_unroll( PhaseIdealLoop *phase ) const {
  CountedLoopNode *cl = _head->as_CountedLoop();
  assert(cl->is_normal_loop(), "");

  Node *init_n = cl->init_trip();
  Node *limit_n = cl->limit();

  // Non-constant bounds
  if (init_n   == NULL || !init_n->is_Con()  ||
      limit_n  == NULL || !limit_n->is_Con() ||
      // protect against stride not being a constant
      !cl->stride_is_con()) {
    return false;
  }
  int init   = init_n->get_int();
  int limit  = limit_n->get_int();
  int span   = limit - init;
  int stride = cl->stride_con();

  if (init >= limit || stride > span) {
    // return a false (no maximally unroll) and the regular unroll/peel
    // route will make a small mess which CCP will fold away.
    return false;
  }
  uint trip_count = span/stride;   // trip_count can be greater than 2 Gig.
  assert( (int)trip_count*stride == span, "must divide evenly" );

  // Real policy: if we maximally unroll, does it get too big?
  // Allow the unrolled mess to get larger than standard loop
  // size.  After all, it will no longer be a loop.
  uint body_size    = _body.size();
  uint unroll_limit = (uint)LoopUnrollLimit * 4;
  assert( (intx)unroll_limit == LoopUnrollLimit * 4, "LoopUnrollLimit must fit in 32bits");
  cl->set_trip_count(trip_count);
  if (trip_count > unroll_limit || body_size > unroll_limit) {
    return false;
  }

  // Currently we don't have policy to optimize one iteration loops.
  // Maximally unrolling transformation is used for that:
  // it is peeled and the original loop become non reachable (dead).
  if (trip_count == 1)
    return true;

  // Do not unroll a loop with String intrinsics code.
  // String intrinsics are large and have loops.
  for (uint k = 0; k < _body.size(); k++) {
    Node* n = _body.at(k);
    switch (n->Opcode()) {
      case Op_StrComp:
      case Op_StrEquals:
      case Op_StrIndexOf:
      case Op_AryEq: {
        return false;
      }
    } // switch
  }

  if (body_size <= unroll_limit) {
    uint new_body_size = body_size * trip_count;
    if (new_body_size <= unroll_limit &&
        body_size == new_body_size / trip_count &&
        // Unrolling can result in a large amount of node construction
        new_body_size < MaxNodeLimit - phase->C->unique()) {
      return true;    // maximally unroll
    }
  }

  return false;               // Do not maximally unroll
}


//------------------------------policy_unroll----------------------------------
// Return TRUE or FALSE if the loop should be unrolled or not.  Unroll if
// the loop is a CountedLoop and the body is small enough.
bool IdealLoopTree::policy_unroll( PhaseIdealLoop *phase ) const {

  CountedLoopNode *cl = _head->as_CountedLoop();
  assert(cl->is_normal_loop() || cl->is_main_loop(), "");

  // protect against stride not being a constant
  if (!cl->stride_is_con()) return false;

  // protect against over-unrolling
  if (cl->trip_count() <= 1) return false;

  int future_unroll_ct = cl->unrolled_count() * 2;

  // Don't unroll if the next round of unrolling would push us
  // over the expected trip count of the loop.  One is subtracted
  // from the expected trip count because the pre-loop normally
  // executes 1 iteration.
  if (UnrollLimitForProfileCheck > 0 &&
      cl->profile_trip_cnt() != COUNT_UNKNOWN &&
      future_unroll_ct        > UnrollLimitForProfileCheck &&
      (float)future_unroll_ct > cl->profile_trip_cnt() - 1.0) {
    return false;
  }

  // When unroll count is greater than LoopUnrollMin, don't unroll if:
  //   the residual iterations are more than 10% of the trip count
  //   and rounds of "unroll,optimize" are not making significant progress
  //   Progress defined as current size less than 20% larger than previous size.
  if (UseSuperWord && cl->node_count_before_unroll() > 0 &&
      future_unroll_ct > LoopUnrollMin &&
      (future_unroll_ct - 1) * 10.0 > cl->profile_trip_cnt() &&
      1.2 * cl->node_count_before_unroll() < (double)_body.size()) {
    return false;
  }

  Node *init_n = cl->init_trip();
  Node *limit_n = cl->limit();
  // Non-constant bounds.
  // Protect against over-unrolling when init or/and limit are not constant
  // (so that trip_count's init value is maxint) but iv range is known.
  if (init_n   == NULL || !init_n->is_Con()  ||
      limit_n  == NULL || !limit_n->is_Con()) {
    Node* phi = cl->phi();
    if (phi != NULL) {
      assert(phi->is_Phi() && phi->in(0) == _head, "Counted loop should have iv phi.");
      const TypeInt* iv_type = phase->_igvn.type(phi)->is_int();
      int next_stride = cl->stride_con() * 2; // stride after this unroll
      if (next_stride > 0) {
        if (iv_type->_lo + next_stride <= iv_type->_lo || // overflow
            iv_type->_lo + next_stride >  iv_type->_hi) {
          return false;  // over-unrolling
        }
      } else if (next_stride < 0) {
        if (iv_type->_hi + next_stride >= iv_type->_hi || // overflow
            iv_type->_hi + next_stride <  iv_type->_lo) {
          return false;  // over-unrolling
        }
      }
    }
  }

  // Adjust body_size to determine if we unroll or not
  uint body_size = _body.size();
  // Key test to unroll CaffeineMark's Logic test
  int xors_in_loop = 0;
  // Also count ModL, DivL and MulL which expand mightly
  for (uint k = 0; k < _body.size(); k++) {
    Node* n = _body.at(k);
    switch (n->Opcode()) {
      case Op_XorI: xors_in_loop++; break; // CaffeineMark's Logic test
      case Op_ModL: body_size += 30; break;
      case Op_DivL: body_size += 30; break;
      case Op_MulL: body_size += 10; break;
      case Op_StrComp:
      case Op_StrEquals:
      case Op_StrIndexOf:
      case Op_AryEq: {
        // Do not unroll a loop with String intrinsics code.
        // String intrinsics are large and have loops.
        return false;
      }
    } // switch
  }

  // Check for being too big
  if (body_size > (uint)LoopUnrollLimit) {
    if (xors_in_loop >= 4 && body_size < (uint)LoopUnrollLimit*4) return true;
    // Normal case: loop too big
    return false;
  }

  // Check for stride being a small enough constant
  if (abs(cl->stride_con()) > (1<<3)) return false;

  // Unroll once!  (Each trip will soon do double iterations)
  return true;
}

//------------------------------policy_align-----------------------------------
// Return TRUE or FALSE if the loop should be cache-line aligned.  Gather the
// expression that does the alignment.  Note that only one array base can be
// aligned in a loop (unless the VM guarantees mutual alignment).  Note that
// if we vectorize short memory ops into longer memory ops, we may want to
// increase alignment.
bool IdealLoopTree::policy_align( PhaseIdealLoop *phase ) const {
  return false;
}

//------------------------------policy_range_check-----------------------------
// Return TRUE or FALSE if the loop should be range-check-eliminated.
// Actually we do iteration-splitting, a more powerful form of RCE.
bool IdealLoopTree::policy_range_check( PhaseIdealLoop *phase ) const {
  if( !RangeCheckElimination ) return false;

  CountedLoopNode *cl = _head->as_CountedLoop();
  // If we unrolled with no intention of doing RCE and we later
  // changed our minds, we got no pre-loop.  Either we need to
  // make a new pre-loop, or we gotta disallow RCE.
  if( cl->is_main_no_pre_loop() ) return false; // Disallowed for now.
  Node *trip_counter = cl->phi();

  // Check loop body for tests of trip-counter plus loop-invariant vs
  // loop-invariant.
  for( uint i = 0; i < _body.size(); i++ ) {
    Node *iff = _body[i];
    if( iff->Opcode() == Op_If ) { // Test?

      // Comparing trip+off vs limit
      Node *bol = iff->in(1);
      if( bol->req() != 2 ) continue; // dead constant test
      if (!bol->is_Bool()) {
        assert(UseLoopPredicate && bol->Opcode() == Op_Conv2B, "predicate check only");
        continue;
      }
      Node *cmp = bol->in(1);

      Node *rc_exp = cmp->in(1);
      Node *limit = cmp->in(2);

      Node *limit_c = phase->get_ctrl(limit);
      if( limit_c == phase->C->top() )
        return false;           // Found dead test on live IF?  No RCE!
      if( is_member(phase->get_loop(limit_c) ) ) {
        // Compare might have operands swapped; commute them
        rc_exp = cmp->in(2);
        limit  = cmp->in(1);
        limit_c = phase->get_ctrl(limit);
        if( is_member(phase->get_loop(limit_c) ) )
          continue;             // Both inputs are loop varying; cannot RCE
      }

      if (!phase->is_scaled_iv_plus_offset(rc_exp, trip_counter, NULL, NULL)) {
        continue;
      }
      // Yeah!  Found a test like 'trip+off vs limit'
      // Test is an IfNode, has 2 projections.  If BOTH are in the loop
      // we need loop unswitching instead of iteration splitting.
      if( is_loop_exit(iff) )
        return true;            // Found reason to split iterations
    } // End of is IF
  }

  return false;
}

//------------------------------policy_peel_only-------------------------------
// Return TRUE or FALSE if the loop should NEVER be RCE'd or aligned.  Useful
// for unrolling loops with NO array accesses.
bool IdealLoopTree::policy_peel_only( PhaseIdealLoop *phase ) const {

  for( uint i = 0; i < _body.size(); i++ )
    if( _body[i]->is_Mem() )
      return false;

  // No memory accesses at all!
  return true;
}

//------------------------------clone_up_backedge_goo--------------------------
// If Node n lives in the back_ctrl block and cannot float, we clone a private
// version of n in preheader_ctrl block and return that, otherwise return n.
Node *PhaseIdealLoop::clone_up_backedge_goo( Node *back_ctrl, Node *preheader_ctrl, Node *n ) {
  if( get_ctrl(n) != back_ctrl ) return n;

  Node *x = NULL;               // If required, a clone of 'n'
  // Check for 'n' being pinned in the backedge.
  if( n->in(0) && n->in(0) == back_ctrl ) {
    x = n->clone();             // Clone a copy of 'n' to preheader
    x->set_req( 0, preheader_ctrl ); // Fix x's control input to preheader
  }

  // Recursive fixup any other input edges into x.
  // If there are no changes we can just return 'n', otherwise
  // we need to clone a private copy and change it.
  for( uint i = 1; i < n->req(); i++ ) {
    Node *g = clone_up_backedge_goo( back_ctrl, preheader_ctrl, n->in(i) );
    if( g != n->in(i) ) {
      if( !x )
        x = n->clone();
      x->set_req(i, g);
    }
  }
  if( x ) {                     // x can legally float to pre-header location
    register_new_node( x, preheader_ctrl );
    return x;
  } else {                      // raise n to cover LCA of uses
    set_ctrl( n, find_non_split_ctrl(back_ctrl->in(0)) );
  }
  return n;
}

//------------------------------insert_pre_post_loops--------------------------
// Insert pre and post loops.  If peel_only is set, the pre-loop can not have
// more iterations added.  It acts as a 'peel' only, no lower-bound RCE, no
// alignment.  Useful to unroll loops that do no array accesses.
void PhaseIdealLoop::insert_pre_post_loops( IdealLoopTree *loop, Node_List &old_new, bool peel_only ) {

#ifndef PRODUCT
  if (TraceLoopOpts) {
    if (peel_only)
      tty->print("PeelMainPost ");
    else
      tty->print("PreMainPost  ");
    loop->dump_head();
  }
#endif
  C->set_major_progress();

  // Find common pieces of the loop being guarded with pre & post loops
  CountedLoopNode *main_head = loop->_head->as_CountedLoop();
  assert( main_head->is_normal_loop(), "" );
  CountedLoopEndNode *main_end = main_head->loopexit();
  assert( main_end->outcnt() == 2, "1 true, 1 false path only" );
  uint dd_main_head = dom_depth(main_head);
  uint max = main_head->outcnt();

  Node *pre_header= main_head->in(LoopNode::EntryControl);
  Node *init      = main_head->init_trip();
  Node *incr      = main_end ->incr();
  Node *limit     = main_end ->limit();
  Node *stride    = main_end ->stride();
  Node *cmp       = main_end ->cmp_node();
  BoolTest::mask b_test = main_end->test_trip();

  // Need only 1 user of 'bol' because I will be hacking the loop bounds.
  Node *bol = main_end->in(CountedLoopEndNode::TestValue);
  if( bol->outcnt() != 1 ) {
    bol = bol->clone();
    register_new_node(bol,main_end->in(CountedLoopEndNode::TestControl));
    _igvn.hash_delete(main_end);
    main_end->set_req(CountedLoopEndNode::TestValue, bol);
  }
  // Need only 1 user of 'cmp' because I will be hacking the loop bounds.
  if( cmp->outcnt() != 1 ) {
    cmp = cmp->clone();
    register_new_node(cmp,main_end->in(CountedLoopEndNode::TestControl));
    _igvn.hash_delete(bol);
    bol->set_req(1, cmp);
  }

  //------------------------------
  // Step A: Create Post-Loop.
  Node* main_exit = main_end->proj_out(false);
  assert( main_exit->Opcode() == Op_IfFalse, "" );
  int dd_main_exit = dom_depth(main_exit);

  // Step A1: Clone the loop body.  The clone becomes the post-loop.  The main
  // loop pre-header illegally has 2 control users (old & new loops).
  clone_loop( loop, old_new, dd_main_exit );
  assert( old_new[main_end ->_idx]->Opcode() == Op_CountedLoopEnd, "" );
  CountedLoopNode *post_head = old_new[main_head->_idx]->as_CountedLoop();
  post_head->set_post_loop(main_head);

  // Reduce the post-loop trip count.
  CountedLoopEndNode* post_end = old_new[main_end ->_idx]->as_CountedLoopEnd();
  post_end->_prob = PROB_FAIR;

  // Build the main-loop normal exit.
  IfFalseNode *new_main_exit = new (C, 1) IfFalseNode(main_end);
  _igvn.register_new_node_with_optimizer( new_main_exit );
  set_idom(new_main_exit, main_end, dd_main_exit );
  set_loop(new_main_exit, loop->_parent);

  // Step A2: Build a zero-trip guard for the post-loop.  After leaving the
  // main-loop, the post-loop may not execute at all.  We 'opaque' the incr
  // (the main-loop trip-counter exit value) because we will be changing
  // the exit value (via unrolling) so we cannot constant-fold away the zero
  // trip guard until all unrolling is done.
  Node *zer_opaq = new (C, 2) Opaque1Node(C, incr);
  Node *zer_cmp  = new (C, 3) CmpINode( zer_opaq, limit );
  Node *zer_bol  = new (C, 2) BoolNode( zer_cmp, b_test );
  register_new_node( zer_opaq, new_main_exit );
  register_new_node( zer_cmp , new_main_exit );
  register_new_node( zer_bol , new_main_exit );

  // Build the IfNode
  IfNode *zer_iff = new (C, 2) IfNode( new_main_exit, zer_bol, PROB_FAIR, COUNT_UNKNOWN );
  _igvn.register_new_node_with_optimizer( zer_iff );
  set_idom(zer_iff, new_main_exit, dd_main_exit);
  set_loop(zer_iff, loop->_parent);

  // Plug in the false-path, taken if we need to skip post-loop
  _igvn.hash_delete( main_exit );
  main_exit->set_req(0, zer_iff);
  _igvn._worklist.push(main_exit);
  set_idom(main_exit, zer_iff, dd_main_exit);
  set_idom(main_exit->unique_out(), zer_iff, dd_main_exit);
  // Make the true-path, must enter the post loop
  Node *zer_taken = new (C, 1) IfTrueNode( zer_iff );
  _igvn.register_new_node_with_optimizer( zer_taken );
  set_idom(zer_taken, zer_iff, dd_main_exit);
  set_loop(zer_taken, loop->_parent);
  // Plug in the true path
  _igvn.hash_delete( post_head );
  post_head->set_req(LoopNode::EntryControl, zer_taken);
  set_idom(post_head, zer_taken, dd_main_exit);

  // Step A3: Make the fall-in values to the post-loop come from the
  // fall-out values of the main-loop.
  for (DUIterator_Fast imax, i = main_head->fast_outs(imax); i < imax; i++) {
    Node* main_phi = main_head->fast_out(i);
    if( main_phi->is_Phi() && main_phi->in(0) == main_head && main_phi->outcnt() >0 ) {
      Node *post_phi = old_new[main_phi->_idx];
      Node *fallmain  = clone_up_backedge_goo(main_head->back_control(),
                                              post_head->init_control(),
                                              main_phi->in(LoopNode::LoopBackControl));
      _igvn.hash_delete(post_phi);
      post_phi->set_req( LoopNode::EntryControl, fallmain );
    }
  }

  // Update local caches for next stanza
  main_exit = new_main_exit;


  //------------------------------
  // Step B: Create Pre-Loop.

  // Step B1: Clone the loop body.  The clone becomes the pre-loop.  The main
  // loop pre-header illegally has 2 control users (old & new loops).
  clone_loop( loop, old_new, dd_main_head );
  CountedLoopNode*    pre_head = old_new[main_head->_idx]->as_CountedLoop();
  CountedLoopEndNode* pre_end  = old_new[main_end ->_idx]->as_CountedLoopEnd();
  pre_head->set_pre_loop(main_head);
  Node *pre_incr = old_new[incr->_idx];

  // Reduce the pre-loop trip count.
  pre_end->_prob = PROB_FAIR;

  // Find the pre-loop normal exit.
  Node* pre_exit = pre_end->proj_out(false);
  assert( pre_exit->Opcode() == Op_IfFalse, "" );
  IfFalseNode *new_pre_exit = new (C, 1) IfFalseNode(pre_end);
  _igvn.register_new_node_with_optimizer( new_pre_exit );
  set_idom(new_pre_exit, pre_end, dd_main_head);
  set_loop(new_pre_exit, loop->_parent);

  // Step B2: Build a zero-trip guard for the main-loop.  After leaving the
  // pre-loop, the main-loop may not execute at all.  Later in life this
  // zero-trip guard will become the minimum-trip guard when we unroll
  // the main-loop.
  Node *min_opaq = new (C, 2) Opaque1Node(C, limit);
  Node *min_cmp  = new (C, 3) CmpINode( pre_incr, min_opaq );
  Node *min_bol  = new (C, 2) BoolNode( min_cmp, b_test );
  register_new_node( min_opaq, new_pre_exit );
  register_new_node( min_cmp , new_pre_exit );
  register_new_node( min_bol , new_pre_exit );

  // Build the IfNode (assume the main-loop is executed always).
  IfNode *min_iff = new (C, 2) IfNode( new_pre_exit, min_bol, PROB_ALWAYS, COUNT_UNKNOWN );
  _igvn.register_new_node_with_optimizer( min_iff );
  set_idom(min_iff, new_pre_exit, dd_main_head);
  set_loop(min_iff, loop->_parent);

  // Plug in the false-path, taken if we need to skip main-loop
  _igvn.hash_delete( pre_exit );
  pre_exit->set_req(0, min_iff);
  set_idom(pre_exit, min_iff, dd_main_head);
  set_idom(pre_exit->unique_out(), min_iff, dd_main_head);
  // Make the true-path, must enter the main loop
  Node *min_taken = new (C, 1) IfTrueNode( min_iff );
  _igvn.register_new_node_with_optimizer( min_taken );
  set_idom(min_taken, min_iff, dd_main_head);
  set_loop(min_taken, loop->_parent);
  // Plug in the true path
  _igvn.hash_delete( main_head );
  main_head->set_req(LoopNode::EntryControl, min_taken);
  set_idom(main_head, min_taken, dd_main_head);

  // Step B3: Make the fall-in values to the main-loop come from the
  // fall-out values of the pre-loop.
  for (DUIterator_Fast i2max, i2 = main_head->fast_outs(i2max); i2 < i2max; i2++) {
    Node* main_phi = main_head->fast_out(i2);
    if( main_phi->is_Phi() && main_phi->in(0) == main_head && main_phi->outcnt() > 0 ) {
      Node *pre_phi = old_new[main_phi->_idx];
      Node *fallpre  = clone_up_backedge_goo(pre_head->back_control(),
                                             main_head->init_control(),
                                             pre_phi->in(LoopNode::LoopBackControl));
      _igvn.hash_delete(main_phi);
      main_phi->set_req( LoopNode::EntryControl, fallpre );
    }
  }

  // Step B4: Shorten the pre-loop to run only 1 iteration (for now).
  // RCE and alignment may change this later.
  Node *cmp_end = pre_end->cmp_node();
  assert( cmp_end->in(2) == limit, "" );
  Node *pre_limit = new (C, 3) AddINode( init, stride );

  // Save the original loop limit in this Opaque1 node for
  // use by range check elimination.
  Node *pre_opaq  = new (C, 3) Opaque1Node(C, pre_limit, limit);

  register_new_node( pre_limit, pre_head->in(0) );
  register_new_node( pre_opaq , pre_head->in(0) );

  // Since no other users of pre-loop compare, I can hack limit directly
  assert( cmp_end->outcnt() == 1, "no other users" );
  _igvn.hash_delete(cmp_end);
  cmp_end->set_req(2, peel_only ? pre_limit : pre_opaq);

  // Special case for not-equal loop bounds:
  // Change pre loop test, main loop test, and the
  // main loop guard test to use lt or gt depending on stride
  // direction:
  // positive stride use <
  // negative stride use >

  if (pre_end->in(CountedLoopEndNode::TestValue)->as_Bool()->_test._test == BoolTest::ne) {

    BoolTest::mask new_test = (main_end->stride_con() > 0) ? BoolTest::lt : BoolTest::gt;
    // Modify pre loop end condition
    Node* pre_bol = pre_end->in(CountedLoopEndNode::TestValue)->as_Bool();
    BoolNode* new_bol0 = new (C, 2) BoolNode(pre_bol->in(1), new_test);
    register_new_node( new_bol0, pre_head->in(0) );
    _igvn.hash_delete(pre_end);
    pre_end->set_req(CountedLoopEndNode::TestValue, new_bol0);
    // Modify main loop guard condition
    assert(min_iff->in(CountedLoopEndNode::TestValue) == min_bol, "guard okay");
    BoolNode* new_bol1 = new (C, 2) BoolNode(min_bol->in(1), new_test);
    register_new_node( new_bol1, new_pre_exit );
    _igvn.hash_delete(min_iff);
    min_iff->set_req(CountedLoopEndNode::TestValue, new_bol1);
    // Modify main loop end condition
    BoolNode* main_bol = main_end->in(CountedLoopEndNode::TestValue)->as_Bool();
    BoolNode* new_bol2 = new (C, 2) BoolNode(main_bol->in(1), new_test);
    register_new_node( new_bol2, main_end->in(CountedLoopEndNode::TestControl) );
    _igvn.hash_delete(main_end);
    main_end->set_req(CountedLoopEndNode::TestValue, new_bol2);
  }

  // Flag main loop
  main_head->set_main_loop();
  if( peel_only ) main_head->set_main_no_pre_loop();

  // It's difficult to be precise about the trip-counts
  // for the pre/post loops.  They are usually very short,
  // so guess that 4 trips is a reasonable value.
  post_head->set_profile_trip_cnt(4.0);
  pre_head->set_profile_trip_cnt(4.0);

  // Now force out all loop-invariant dominating tests.  The optimizer
  // finds some, but we _know_ they are all useless.
  peeled_dom_test_elim(loop,old_new);
}

//------------------------------is_invariant-----------------------------
// Return true if n is invariant
bool IdealLoopTree::is_invariant(Node* n) const {
  Node *n_c = _phase->has_ctrl(n) ? _phase->get_ctrl(n) : n;
  if (n_c->is_top()) return false;
  return !is_member(_phase->get_loop(n_c));
}


//------------------------------do_unroll--------------------------------------
// Unroll the loop body one step - make each trip do 2 iterations.
void PhaseIdealLoop::do_unroll( IdealLoopTree *loop, Node_List &old_new, bool adjust_min_trip ) {
  assert(LoopUnrollLimit, "");
  CountedLoopNode *loop_head = loop->_head->as_CountedLoop();
  CountedLoopEndNode *loop_end = loop_head->loopexit();
  assert(loop_end, "");
#ifndef PRODUCT
  if (PrintOpto && VerifyLoopOptimizations) {
    tty->print("Unrolling ");
    loop->dump_head();
  } else if (TraceLoopOpts) {
    tty->print("Unroll     %d ", loop_head->unrolled_count()*2);
    loop->dump_head();
  }
#endif

  // Remember loop node count before unrolling to detect
  // if rounds of unroll,optimize are making progress
  loop_head->set_node_count_before_unroll(loop->_body.size());

  Node *ctrl  = loop_head->in(LoopNode::EntryControl);
  Node *limit = loop_head->limit();
  Node *init  = loop_head->init_trip();
  Node *stride = loop_head->stride();

  Node *opaq = NULL;
  if( adjust_min_trip ) {       // If not maximally unrolling, need adjustment
    assert( loop_head->is_main_loop(), "" );
    assert( ctrl->Opcode() == Op_IfTrue || ctrl->Opcode() == Op_IfFalse, "" );
    Node *iff = ctrl->in(0);
    assert( iff->Opcode() == Op_If, "" );
    Node *bol = iff->in(1);
    assert( bol->Opcode() == Op_Bool, "" );
    Node *cmp = bol->in(1);
    assert( cmp->Opcode() == Op_CmpI, "" );
    opaq = cmp->in(2);
    // Occasionally it's possible for a pre-loop Opaque1 node to be
    // optimized away and then another round of loop opts attempted.
    // We can not optimize this particular loop in that case.
    if( opaq->Opcode() != Op_Opaque1 )
      return;                   // Cannot find pre-loop!  Bail out!
  }

  C->set_major_progress();

  // Adjust max trip count. The trip count is intentionally rounded
  // down here (e.g. 15-> 7-> 3-> 1) because if we unwittingly over-unroll,
  // the main, unrolled, part of the loop will never execute as it is protected
  // by the min-trip test.  See bug 4834191 for a case where we over-unrolled
  // and later determined that part of the unrolled loop was dead.
  loop_head->set_trip_count(loop_head->trip_count() / 2);

  // Double the count of original iterations in the unrolled loop body.
  loop_head->double_unrolled_count();

  // -----------
  // Step 2: Cut back the trip counter for an unroll amount of 2.
  // Loop will normally trip (limit - init)/stride_con.  Since it's a
  // CountedLoop this is exact (stride divides limit-init exactly).
  // We are going to double the loop body, so we want to knock off any
  // odd iteration: (trip_cnt & ~1).  Then back compute a new limit.
  Node *span = new (C, 3) SubINode( limit, init );
  register_new_node( span, ctrl );
  Node *trip = new (C, 3) DivINode( 0, span, stride );
  register_new_node( trip, ctrl );
  Node *mtwo = _igvn.intcon(-2);
  set_ctrl(mtwo, C->root());
  Node *rond = new (C, 3) AndINode( trip, mtwo );
  register_new_node( rond, ctrl );
  Node *spn2 = new (C, 3) MulINode( rond, stride );
  register_new_node( spn2, ctrl );
  Node *lim2 = new (C, 3) AddINode( spn2, init );
  register_new_node( lim2, ctrl );

  // Hammer in the new limit
  Node *ctrl2 = loop_end->in(0);
  Node *cmp2 = new (C, 3) CmpINode( loop_head->incr(), lim2 );
  register_new_node( cmp2, ctrl2 );
  Node *bol2 = new (C, 2) BoolNode( cmp2, loop_end->test_trip() );
  register_new_node( bol2, ctrl2 );
  _igvn.hash_delete(loop_end);
  loop_end->set_req(CountedLoopEndNode::TestValue, bol2);

  // Step 3: Find the min-trip test guaranteed before a 'main' loop.
  // Make it a 1-trip test (means at least 2 trips).
  if( adjust_min_trip ) {
    // Guard test uses an 'opaque' node which is not shared.  Hence I
    // can edit it's inputs directly.  Hammer in the new limit for the
    // minimum-trip guard.
    assert( opaq->outcnt() == 1, "" );
    _igvn.hash_delete(opaq);
    opaq->set_req(1, lim2);
  }

  // ---------
  // Step 4: Clone the loop body.  Move it inside the loop.  This loop body
  // represents the odd iterations; since the loop trips an even number of
  // times its backedge is never taken.  Kill the backedge.
  uint dd = dom_depth(loop_head);
  clone_loop( loop, old_new, dd );

  // Make backedges of the clone equal to backedges of the original.
  // Make the fall-in from the original come from the fall-out of the clone.
  for (DUIterator_Fast jmax, j = loop_head->fast_outs(jmax); j < jmax; j++) {
    Node* phi = loop_head->fast_out(j);
    if( phi->is_Phi() && phi->in(0) == loop_head && phi->outcnt() > 0 ) {
      Node *newphi = old_new[phi->_idx];
      _igvn.hash_delete( phi );
      _igvn.hash_delete( newphi );

      phi   ->set_req(LoopNode::   EntryControl, newphi->in(LoopNode::LoopBackControl));
      newphi->set_req(LoopNode::LoopBackControl, phi   ->in(LoopNode::LoopBackControl));
      phi   ->set_req(LoopNode::LoopBackControl, C->top());
    }
  }
  Node *clone_head = old_new[loop_head->_idx];
  _igvn.hash_delete( clone_head );
  loop_head ->set_req(LoopNode::   EntryControl, clone_head->in(LoopNode::LoopBackControl));
  clone_head->set_req(LoopNode::LoopBackControl, loop_head ->in(LoopNode::LoopBackControl));
  loop_head ->set_req(LoopNode::LoopBackControl, C->top());
  loop->_head = clone_head;     // New loop header

  set_idom(loop_head,  loop_head ->in(LoopNode::EntryControl), dd);
  set_idom(clone_head, clone_head->in(LoopNode::EntryControl), dd);

  // Kill the clone's backedge
  Node *newcle = old_new[loop_end->_idx];
  _igvn.hash_delete( newcle );
  Node *one = _igvn.intcon(1);
  set_ctrl(one, C->root());
  newcle->set_req(1, one);
  // Force clone into same loop body
  uint max = loop->_body.size();
  for( uint k = 0; k < max; k++ ) {
    Node *old = loop->_body.at(k);
    Node *nnn = old_new[old->_idx];
    loop->_body.push(nnn);
    if (!has_ctrl(old))
      set_loop(nnn, loop);
  }

  loop->record_for_igvn();
}

//------------------------------do_maximally_unroll----------------------------

void PhaseIdealLoop::do_maximally_unroll( IdealLoopTree *loop, Node_List &old_new ) {
  CountedLoopNode *cl = loop->_head->as_CountedLoop();
  assert(cl->trip_count() > 0, "");
#ifndef PRODUCT
  if (TraceLoopOpts) {
    tty->print("MaxUnroll  %d ", cl->trip_count());
    loop->dump_head();
  }
#endif

  // If loop is tripping an odd number of times, peel odd iteration
  if ((cl->trip_count() & 1) == 1) {
    do_peeling(loop, old_new);
  }

  // Now its tripping an even number of times remaining.  Double loop body.
  // Do not adjust pre-guards; they are not needed and do not exist.
  if (cl->trip_count() > 0) {
    do_unroll(loop, old_new, false);
  }
}

//------------------------------dominates_backedge---------------------------------
// Returns true if ctrl is executed on every complete iteration
bool IdealLoopTree::dominates_backedge(Node* ctrl) {
  assert(ctrl->is_CFG(), "must be control");
  Node* backedge = _head->as_Loop()->in(LoopNode::LoopBackControl);
  return _phase->dom_lca_internal(ctrl, backedge) == ctrl;
}

//------------------------------add_constraint---------------------------------
// Constrain the main loop iterations so the condition:
//    scale_con * I + offset  <  limit
// always holds true.  That is, either increase the number of iterations in
// the pre-loop or the post-loop until the condition holds true in the main
// loop.  Stride, scale, offset and limit are all loop invariant.  Further,
// stride and scale are constants (offset and limit often are).
void PhaseIdealLoop::add_constraint( int stride_con, int scale_con, Node *offset, Node *limit, Node *pre_ctrl, Node **pre_limit, Node **main_limit ) {

  // Compute "I :: (limit-offset)/scale_con"
  Node *con = new (C, 3) SubINode( limit, offset );
  register_new_node( con, pre_ctrl );
  Node *scale = _igvn.intcon(scale_con);
  set_ctrl(scale, C->root());
  Node *X = new (C, 3) DivINode( 0, con, scale );
  register_new_node( X, pre_ctrl );

  // For positive stride, the pre-loop limit always uses a MAX function
  // and the main loop a MIN function.  For negative stride these are
  // reversed.

  // Also for positive stride*scale the affine function is increasing, so the
  // pre-loop must check for underflow and the post-loop for overflow.
  // Negative stride*scale reverses this; pre-loop checks for overflow and
  // post-loop for underflow.
  if( stride_con*scale_con > 0 ) {
    // Compute I < (limit-offset)/scale_con
    // Adjust main-loop last iteration to be MIN/MAX(main_loop,X)
    *main_limit = (stride_con > 0)
      ? (Node*)(new (C, 3) MinINode( *main_limit, X ))
      : (Node*)(new (C, 3) MaxINode( *main_limit, X ));
    register_new_node( *main_limit, pre_ctrl );

  } else {
    // Compute (limit-offset)/scale_con + SGN(-scale_con) <= I
    // Add the negation of the main-loop constraint to the pre-loop.
    // See footnote [++] below for a derivation of the limit expression.
    Node *incr = _igvn.intcon(scale_con > 0 ? -1 : 1);
    set_ctrl(incr, C->root());
    Node *adj = new (C, 3) AddINode( X, incr );
    register_new_node( adj, pre_ctrl );
    *pre_limit = (scale_con > 0)
      ? (Node*)new (C, 3) MinINode( *pre_limit, adj )
      : (Node*)new (C, 3) MaxINode( *pre_limit, adj );
    register_new_node( *pre_limit, pre_ctrl );

//   [++] Here's the algebra that justifies the pre-loop limit expression:
//
//   NOT( scale_con * I + offset  <  limit )
//      ==
//   scale_con * I + offset  >=  limit
//      ==
//   SGN(scale_con) * I  >=  (limit-offset)/|scale_con|
//      ==
//   (limit-offset)/|scale_con|   <=  I * SGN(scale_con)
//      ==
//   (limit-offset)/|scale_con|-1  <  I * SGN(scale_con)
//      ==
//   ( if (scale_con > 0) /*common case*/
//       (limit-offset)/scale_con - 1  <  I
//     else
//       (limit-offset)/scale_con + 1  >  I
//    )
//   ( if (scale_con > 0) /*common case*/
//       (limit-offset)/scale_con + SGN(-scale_con)  <  I
//     else
//       (limit-offset)/scale_con + SGN(-scale_con)  >  I
  }
}


//------------------------------is_scaled_iv---------------------------------
// Return true if exp is a constant times an induction var
bool PhaseIdealLoop::is_scaled_iv(Node* exp, Node* iv, int* p_scale) {
  if (exp == iv) {
    if (p_scale != NULL) {
      *p_scale = 1;
    }
    return true;
  }
  int opc = exp->Opcode();
  if (opc == Op_MulI) {
    if (exp->in(1) == iv && exp->in(2)->is_Con()) {
      if (p_scale != NULL) {
        *p_scale = exp->in(2)->get_int();
      }
      return true;
    }
    if (exp->in(2) == iv && exp->in(1)->is_Con()) {
      if (p_scale != NULL) {
        *p_scale = exp->in(1)->get_int();
      }
      return true;
    }
  } else if (opc == Op_LShiftI) {
    if (exp->in(1) == iv && exp->in(2)->is_Con()) {
      if (p_scale != NULL) {
        *p_scale = 1 << exp->in(2)->get_int();
      }
      return true;
    }
  }
  return false;
}

//-----------------------------is_scaled_iv_plus_offset------------------------------
// Return true if exp is a simple induction variable expression: k1*iv + (invar + k2)
bool PhaseIdealLoop::is_scaled_iv_plus_offset(Node* exp, Node* iv, int* p_scale, Node** p_offset, int depth) {
  if (is_scaled_iv(exp, iv, p_scale)) {
    if (p_offset != NULL) {
      Node *zero = _igvn.intcon(0);
      set_ctrl(zero, C->root());
      *p_offset = zero;
    }
    return true;
  }
  int opc = exp->Opcode();
  if (opc == Op_AddI) {
    if (is_scaled_iv(exp->in(1), iv, p_scale)) {
      if (p_offset != NULL) {
        *p_offset = exp->in(2);
      }
      return true;
    }
    if (exp->in(2)->is_Con()) {
      Node* offset2 = NULL;
      if (depth < 2 &&
          is_scaled_iv_plus_offset(exp->in(1), iv, p_scale,
                                   p_offset != NULL ? &offset2 : NULL, depth+1)) {
        if (p_offset != NULL) {
          Node *ctrl_off2 = get_ctrl(offset2);
          Node* offset = new (C, 3) AddINode(offset2, exp->in(2));
          register_new_node(offset, ctrl_off2);
          *p_offset = offset;
        }
        return true;
      }
    }
  } else if (opc == Op_SubI) {
    if (is_scaled_iv(exp->in(1), iv, p_scale)) {
      if (p_offset != NULL) {
        Node *zero = _igvn.intcon(0);
        set_ctrl(zero, C->root());
        Node *ctrl_off = get_ctrl(exp->in(2));
        Node* offset = new (C, 3) SubINode(zero, exp->in(2));
        register_new_node(offset, ctrl_off);
        *p_offset = offset;
      }
      return true;
    }
    if (is_scaled_iv(exp->in(2), iv, p_scale)) {
      if (p_offset != NULL) {
        *p_scale *= -1;
        *p_offset = exp->in(1);
      }
      return true;
    }
  }
  return false;
}

//------------------------------do_range_check---------------------------------
// Eliminate range-checks and other trip-counter vs loop-invariant tests.
void PhaseIdealLoop::do_range_check( IdealLoopTree *loop, Node_List &old_new ) {
#ifndef PRODUCT
  if (PrintOpto && VerifyLoopOptimizations) {
    tty->print("Range Check Elimination ");
    loop->dump_head();
  } else if (TraceLoopOpts) {
    tty->print("RangeCheck   ");
    loop->dump_head();
  }
#endif
  assert(RangeCheckElimination, "");
  CountedLoopNode *cl = loop->_head->as_CountedLoop();
  assert(cl->is_main_loop(), "");

  // protect against stride not being a constant
  if (!cl->stride_is_con())
    return;

  // Find the trip counter; we are iteration splitting based on it
  Node *trip_counter = cl->phi();
  // Find the main loop limit; we will trim it's iterations
  // to not ever trip end tests
  Node *main_limit = cl->limit();

  // Need to find the main-loop zero-trip guard
  Node *ctrl  = cl->in(LoopNode::EntryControl);
  assert(ctrl->Opcode() == Op_IfTrue || ctrl->Opcode() == Op_IfFalse, "");
  Node *iffm = ctrl->in(0);
  assert(iffm->Opcode() == Op_If, "");
  Node *bolzm = iffm->in(1);
  assert(bolzm->Opcode() == Op_Bool, "");
  Node *cmpzm = bolzm->in(1);
  assert(cmpzm->is_Cmp(), "");
  Node *opqzm = cmpzm->in(2);
  // Can not optimize a loop if pre-loop Opaque1 node is optimized
  // away and then another round of loop opts attempted.
  if (opqzm->Opcode() != Op_Opaque1)
    return;
  assert(opqzm->in(1) == main_limit, "do not understand situation");

  // Find the pre-loop limit; we will expand it's iterations to
  // not ever trip low tests.
  Node *p_f = iffm->in(0);
  assert(p_f->Opcode() == Op_IfFalse, "");
  CountedLoopEndNode *pre_end = p_f->in(0)->as_CountedLoopEnd();
  assert(pre_end->loopnode()->is_pre_loop(), "");
  Node *pre_opaq1 = pre_end->limit();
  // Occasionally it's possible for a pre-loop Opaque1 node to be
  // optimized away and then another round of loop opts attempted.
  // We can not optimize this particular loop in that case.
  if (pre_opaq1->Opcode() != Op_Opaque1)
    return;
  Opaque1Node *pre_opaq = (Opaque1Node*)pre_opaq1;
  Node *pre_limit = pre_opaq->in(1);

  // Where do we put new limit calculations
  Node *pre_ctrl = pre_end->loopnode()->in(LoopNode::EntryControl);

  // Ensure the original loop limit is available from the
  // pre-loop Opaque1 node.
  Node *orig_limit = pre_opaq->original_loop_limit();
  if (orig_limit == NULL || _igvn.type(orig_limit) == Type::TOP)
    return;

  // Must know if its a count-up or count-down loop

  int stride_con = cl->stride_con();
  Node *zero = _igvn.intcon(0);
  Node *one  = _igvn.intcon(1);
  set_ctrl(zero, C->root());
  set_ctrl(one,  C->root());

  // Range checks that do not dominate the loop backedge (ie.
  // conditionally executed) can lengthen the pre loop limit beyond
  // the original loop limit. To prevent this, the pre limit is
  // (for stride > 0) MINed with the original loop limit (MAXed
  // stride < 0) when some range_check (rc) is conditionally
  // executed.
  bool conditional_rc = false;

  // Check loop body for tests of trip-counter plus loop-invariant vs
  // loop-invariant.
  for( uint i = 0; i < loop->_body.size(); i++ ) {
    Node *iff = loop->_body[i];
    if( iff->Opcode() == Op_If ) { // Test?

      // Test is an IfNode, has 2 projections.  If BOTH are in the loop
      // we need loop unswitching instead of iteration splitting.
      Node *exit = loop->is_loop_exit(iff);
      if( !exit ) continue;
      int flip = (exit->Opcode() == Op_IfTrue) ? 1 : 0;

      // Get boolean condition to test
      Node *i1 = iff->in(1);
      if( !i1->is_Bool() ) continue;
      BoolNode *bol = i1->as_Bool();
      BoolTest b_test = bol->_test;
      // Flip sense of test if exit condition is flipped
      if( flip )
        b_test = b_test.negate();

      // Get compare
      Node *cmp = bol->in(1);

      // Look for trip_counter + offset vs limit
      Node *rc_exp = cmp->in(1);
      Node *limit  = cmp->in(2);
      jint scale_con= 1;        // Assume trip counter not scaled

      Node *limit_c = get_ctrl(limit);
      if( loop->is_member(get_loop(limit_c) ) ) {
        // Compare might have operands swapped; commute them
        b_test = b_test.commute();
        rc_exp = cmp->in(2);
        limit  = cmp->in(1);
        limit_c = get_ctrl(limit);
        if( loop->is_member(get_loop(limit_c) ) )
          continue;             // Both inputs are loop varying; cannot RCE
      }
      // Here we know 'limit' is loop invariant

      // 'limit' maybe pinned below the zero trip test (probably from a
      // previous round of rce), in which case, it can't be used in the
      // zero trip test expression which must occur before the zero test's if.
      if( limit_c == ctrl ) {
        continue;  // Don't rce this check but continue looking for other candidates.
      }

      // Check for scaled induction variable plus an offset
      Node *offset = NULL;

      if (!is_scaled_iv_plus_offset(rc_exp, trip_counter, &scale_con, &offset)) {
        continue;
      }

      Node *offset_c = get_ctrl(offset);
      if( loop->is_member( get_loop(offset_c) ) )
        continue;               // Offset is not really loop invariant
      // Here we know 'offset' is loop invariant.

      // As above for the 'limit', the 'offset' maybe pinned below the
      // zero trip test.
      if( offset_c == ctrl ) {
        continue; // Don't rce this check but continue looking for other candidates.
      }

      // At this point we have the expression as:
      //   scale_con * trip_counter + offset :: limit
      // where scale_con, offset and limit are loop invariant.  Trip_counter
      // monotonically increases by stride_con, a constant.  Both (or either)
      // stride_con and scale_con can be negative which will flip about the
      // sense of the test.

      // Adjust pre and main loop limits to guard the correct iteration set
      if( cmp->Opcode() == Op_CmpU ) {// Unsigned compare is really 2 tests
        if( b_test._test == BoolTest::lt ) { // Range checks always use lt
          // The overflow limit: scale*I+offset < limit
          add_constraint( stride_con, scale_con, offset, limit, pre_ctrl, &pre_limit, &main_limit );
          // The underflow limit: 0 <= scale*I+offset.
          // Some math yields: -scale*I-(offset+1) < 0
          Node *plus_one = new (C, 3) AddINode( offset, one );
          register_new_node( plus_one, pre_ctrl );
          Node *neg_offset = new (C, 3) SubINode( zero, plus_one );
          register_new_node( neg_offset, pre_ctrl );
          add_constraint( stride_con, -scale_con, neg_offset, zero, pre_ctrl, &pre_limit, &main_limit );
          if (!conditional_rc) {
            conditional_rc = !loop->dominates_backedge(iff);
          }
        } else {
#ifndef PRODUCT
          if( PrintOpto )
            tty->print_cr("missed RCE opportunity");
#endif
          continue;             // In release mode, ignore it
        }
      } else {                  // Otherwise work on normal compares
        switch( b_test._test ) {
        case BoolTest::ge:      // Convert X >= Y to -X <= -Y
          scale_con = -scale_con;
          offset = new (C, 3) SubINode( zero, offset );
          register_new_node( offset, pre_ctrl );
          limit  = new (C, 3) SubINode( zero, limit  );
          register_new_node( limit, pre_ctrl );
          // Fall into LE case
        case BoolTest::le:      // Convert X <= Y to X < Y+1
          limit = new (C, 3) AddINode( limit, one );
          register_new_node( limit, pre_ctrl );
          // Fall into LT case
        case BoolTest::lt:
          add_constraint( stride_con, scale_con, offset, limit, pre_ctrl, &pre_limit, &main_limit );
          if (!conditional_rc) {
            conditional_rc = !loop->dominates_backedge(iff);
          }
          break;
        default:
#ifndef PRODUCT
          if( PrintOpto )
            tty->print_cr("missed RCE opportunity");
#endif
          continue;             // Unhandled case
        }
      }

      // Kill the eliminated test
      C->set_major_progress();
      Node *kill_con = _igvn.intcon( 1-flip );
      set_ctrl(kill_con, C->root());
      _igvn.hash_delete(iff);
      iff->set_req(1, kill_con);
      _igvn._worklist.push(iff);
      // Find surviving projection
      assert(iff->is_If(), "");
      ProjNode* dp = ((IfNode*)iff)->proj_out(1-flip);
      // Find loads off the surviving projection; remove their control edge
      for (DUIterator_Fast imax, i = dp->fast_outs(imax); i < imax; i++) {
        Node* cd = dp->fast_out(i); // Control-dependent node
        if( cd->is_Load() ) {   // Loads can now float around in the loop
          _igvn.hash_delete(cd);
          // Allow the load to float around in the loop, or before it
          // but NOT before the pre-loop.
          cd->set_req(0, ctrl);   // ctrl, not NULL
          _igvn._worklist.push(cd);
          --i;
          --imax;
        }
      }

    } // End of is IF

  }

  // Update loop limits
  if (conditional_rc) {
    pre_limit = (stride_con > 0) ? (Node*)new (C,3) MinINode(pre_limit, orig_limit)
                                 : (Node*)new (C,3) MaxINode(pre_limit, orig_limit);
    register_new_node(pre_limit, pre_ctrl);
  }
  _igvn.hash_delete(pre_opaq);
  pre_opaq->set_req(1, pre_limit);

  // Note:: we are making the main loop limit no longer precise;
  // need to round up based on stride.
  if( stride_con != 1 && stride_con != -1 ) { // Cutout for common case
    // "Standard" round-up logic:  ([main_limit-init+(y-1)]/y)*y+init
    // Hopefully, compiler will optimize for powers of 2.
    Node *ctrl = get_ctrl(main_limit);
    Node *stride = cl->stride();
    Node *init = cl->init_trip();
    Node *span = new (C, 3) SubINode(main_limit,init);
    register_new_node(span,ctrl);
    Node *rndup = _igvn.intcon(stride_con + ((stride_con>0)?-1:1));
    Node *add = new (C, 3) AddINode(span,rndup);
    register_new_node(add,ctrl);
    Node *div = new (C, 3) DivINode(0,add,stride);
    register_new_node(div,ctrl);
    Node *mul = new (C, 3) MulINode(div,stride);
    register_new_node(mul,ctrl);
    Node *newlim = new (C, 3) AddINode(mul,init);
    register_new_node(newlim,ctrl);
    main_limit = newlim;
  }

  Node *main_cle = cl->loopexit();
  Node *main_bol = main_cle->in(1);
  // Hacking loop bounds; need private copies of exit test
  if( main_bol->outcnt() > 1 ) {// BoolNode shared?
    _igvn.hash_delete(main_cle);
    main_bol = main_bol->clone();// Clone a private BoolNode
    register_new_node( main_bol, main_cle->in(0) );
    main_cle->set_req(1,main_bol);
  }
  Node *main_cmp = main_bol->in(1);
  if( main_cmp->outcnt() > 1 ) { // CmpNode shared?
    _igvn.hash_delete(main_bol);
    main_cmp = main_cmp->clone();// Clone a private CmpNode
    register_new_node( main_cmp, main_cle->in(0) );
    main_bol->set_req(1,main_cmp);
  }
  // Hack the now-private loop bounds
  _igvn.hash_delete(main_cmp);
  main_cmp->set_req(2, main_limit);
  _igvn._worklist.push(main_cmp);
  // The OpaqueNode is unshared by design
  _igvn.hash_delete(opqzm);
  assert( opqzm->outcnt() == 1, "cannot hack shared node" );
  opqzm->set_req(1,main_limit);
  _igvn._worklist.push(opqzm);
}

//------------------------------DCE_loop_body----------------------------------
// Remove simplistic dead code from loop body
void IdealLoopTree::DCE_loop_body() {
  for( uint i = 0; i < _body.size(); i++ )
    if( _body.at(i)->outcnt() == 0 )
      _body.map( i--, _body.pop() );
}


//------------------------------adjust_loop_exit_prob--------------------------
// Look for loop-exit tests with the 50/50 (or worse) guesses from the parsing stage.
// Replace with a 1-in-10 exit guess.
void IdealLoopTree::adjust_loop_exit_prob( PhaseIdealLoop *phase ) {
  Node *test = tail();
  while( test != _head ) {
    uint top = test->Opcode();
    if( top == Op_IfTrue || top == Op_IfFalse ) {
      int test_con = ((ProjNode*)test)->_con;
      assert(top == (uint)(test_con? Op_IfTrue: Op_IfFalse), "sanity");
      IfNode *iff = test->in(0)->as_If();
      if( iff->outcnt() == 2 ) {        // Ignore dead tests
        Node *bol = iff->in(1);
        if( bol && bol->req() > 1 && bol->in(1) &&
            ((bol->in(1)->Opcode() == Op_StorePConditional ) ||
             (bol->in(1)->Opcode() == Op_StoreIConditional ) ||
             (bol->in(1)->Opcode() == Op_StoreLConditional ) ||
             (bol->in(1)->Opcode() == Op_CompareAndSwapI ) ||
             (bol->in(1)->Opcode() == Op_CompareAndSwapL ) ||
             (bol->in(1)->Opcode() == Op_CompareAndSwapP ) ||
             (bol->in(1)->Opcode() == Op_CompareAndSwapN )))
          return;               // Allocation loops RARELY take backedge
        // Find the OTHER exit path from the IF
        Node* ex = iff->proj_out(1-test_con);
        float p = iff->_prob;
        if( !phase->is_member( this, ex ) && iff->_fcnt == COUNT_UNKNOWN ) {
          if( top == Op_IfTrue ) {
            if( p < (PROB_FAIR + PROB_UNLIKELY_MAG(3))) {
              iff->_prob = PROB_STATIC_FREQUENT;
            }
          } else {
            if( p > (PROB_FAIR - PROB_UNLIKELY_MAG(3))) {
              iff->_prob = PROB_STATIC_INFREQUENT;
            }
          }
        }
      }
    }
    test = phase->idom(test);
  }
}


//------------------------------policy_do_remove_empty_loop--------------------
// Micro-benchmark spamming.  Policy is to always remove empty loops.
// The 'DO' part is to replace the trip counter with the value it will
// have on the last iteration.  This will break the loop.
bool IdealLoopTree::policy_do_remove_empty_loop( PhaseIdealLoop *phase ) {
  // Minimum size must be empty loop
  if (_body.size() > 7/*number of nodes in an empty loop*/)
    return false;

  if (!_head->is_CountedLoop())
    return false;     // Dead loop
  CountedLoopNode *cl = _head->as_CountedLoop();
  if (!cl->loopexit())
    return false; // Malformed loop
  if (!phase->is_member(this, phase->get_ctrl(cl->loopexit()->in(CountedLoopEndNode::TestValue))))
    return false;             // Infinite loop

#ifdef ASSERT
  // Ensure only one phi which is the iv.
  Node* iv = NULL;
  for (DUIterator_Fast imax, i = cl->fast_outs(imax); i < imax; i++) {
    Node* n = cl->fast_out(i);
    if (n->Opcode() == Op_Phi) {
      assert(iv == NULL, "Too many phis" );
      iv = n;
    }
  }
  assert(iv == cl->phi(), "Wrong phi" );
#endif

  // main and post loops have explicitly created zero trip guard
  bool needs_guard = !cl->is_main_loop() && !cl->is_post_loop();
  if (needs_guard) {
    // Check for an obvious zero trip guard.
    Node* inctrl = cl->in(LoopNode::EntryControl);
    if (inctrl->Opcode() == Op_IfTrue) {
      // The test should look like just the backedge of a CountedLoop
      Node* iff = inctrl->in(0);
      if (iff->is_If()) {
        Node* bol = iff->in(1);
        if (bol->is_Bool() && bol->as_Bool()->_test._test == cl->loopexit()->test_trip()) {
          Node* cmp = bol->in(1);
          if (cmp->is_Cmp() && cmp->in(1) == cl->init_trip() && cmp->in(2) == cl->limit()) {
            needs_guard = false;
          }
        }
      }
    }
  }

#ifndef PRODUCT
  if (PrintOpto) {
    tty->print("Removing empty loop with%s zero trip guard", needs_guard ? "out" : "");
    this->dump_head();
  } else if (TraceLoopOpts) {
    tty->print("Empty with%s zero trip guard   ", needs_guard ? "out" : "");
    this->dump_head();
  }
#endif

  if (needs_guard) {
    // Peel the loop to ensure there's a zero trip guard
    Node_List old_new;
    phase->do_peeling(this, old_new);
  }

  // Replace the phi at loop head with the final value of the last
  // iteration.  Then the CountedLoopEnd will collapse (backedge never
  // taken) and all loop-invariant uses of the exit values will be correct.
  Node *phi = cl->phi();
  Node *final = new (phase->C, 3) SubINode( cl->limit(), cl->stride() );
  phase->register_new_node(final,cl->in(LoopNode::EntryControl));
  phase->_igvn.replace_node(phi,final);
  phase->C->set_major_progress();
  return true;
}


//=============================================================================
//------------------------------iteration_split_impl---------------------------
bool IdealLoopTree::iteration_split_impl( PhaseIdealLoop *phase, Node_List &old_new ) {
  // Check and remove empty loops (spam micro-benchmarks)
  if( policy_do_remove_empty_loop(phase) )
    return true;  // Here we removed an empty loop

  bool should_peel = policy_peeling(phase); // Should we peel?

  bool should_unswitch = policy_unswitching(phase);

  // Non-counted loops may be peeled; exactly 1 iteration is peeled.
  // This removes loop-invariant tests (usually null checks).
  if( !_head->is_CountedLoop() ) { // Non-counted loop
    if (PartialPeelLoop && phase->partial_peel(this, old_new)) {
      // Partial peel succeeded so terminate this round of loop opts
      return false;
    }
    if( should_peel ) {            // Should we peel?
#ifndef PRODUCT
      if (PrintOpto) tty->print_cr("should_peel");
#endif
      phase->do_peeling(this,old_new);
    } else if( should_unswitch ) {
      phase->do_unswitching(this, old_new);
    }
    return true;
  }
  CountedLoopNode *cl = _head->as_CountedLoop();

  if( !cl->loopexit() ) return true; // Ignore various kinds of broken loops

  // Do nothing special to pre- and post- loops
  if( cl->is_pre_loop() || cl->is_post_loop() ) return true;

  // Compute loop trip count from profile data
  compute_profile_trip_cnt(phase);

  // Before attempting fancy unrolling, RCE or alignment, see if we want
  // to completely unroll this loop or do loop unswitching.
  if( cl->is_normal_loop() ) {
    if (should_unswitch) {
      phase->do_unswitching(this, old_new);
      return true;
    }
    bool should_maximally_unroll =  policy_maximally_unroll(phase);
    if( should_maximally_unroll ) {
      // Here we did some unrolling and peeling.  Eventually we will
      // completely unroll this loop and it will no longer be a loop.
      phase->do_maximally_unroll(this,old_new);
      return true;
    }
  }


  // Counted loops may be peeled, may need some iterations run up
  // front for RCE, and may want to align loop refs to a cache
  // line.  Thus we clone a full loop up front whose trip count is
  // at least 1 (if peeling), but may be several more.

  // The main loop will start cache-line aligned with at least 1
  // iteration of the unrolled body (zero-trip test required) and
  // will have some range checks removed.

  // A post-loop will finish any odd iterations (leftover after
  // unrolling), plus any needed for RCE purposes.

  bool should_unroll = policy_unroll(phase);

  bool should_rce = policy_range_check(phase);

  bool should_align = policy_align(phase);

  // If not RCE'ing (iteration splitting) or Aligning, then we do not
  // need a pre-loop.  We may still need to peel an initial iteration but
  // we will not be needing an unknown number of pre-iterations.
  //
  // Basically, if may_rce_align reports FALSE first time through,
  // we will not be able to later do RCE or Aligning on this loop.
  bool may_rce_align = !policy_peel_only(phase) || should_rce || should_align;

  // If we have any of these conditions (RCE, alignment, unrolling) met, then
  // we switch to the pre-/main-/post-loop model.  This model also covers
  // peeling.
  if( should_rce || should_align || should_unroll ) {
    if( cl->is_normal_loop() )  // Convert to 'pre/main/post' loops
      phase->insert_pre_post_loops(this,old_new, !may_rce_align);

    // Adjust the pre- and main-loop limits to let the pre and post loops run
    // with full checks, but the main-loop with no checks.  Remove said
    // checks from the main body.
    if( should_rce )
      phase->do_range_check(this,old_new);

    // Double loop body for unrolling.  Adjust the minimum-trip test (will do
    // twice as many iterations as before) and the main body limit (only do
    // an even number of trips).  If we are peeling, we might enable some RCE
    // and we'd rather unroll the post-RCE'd loop SO... do not unroll if
    // peeling.
      if( should_unroll && !should_peel )
        phase->do_unroll(this,old_new, true);

    // Adjust the pre-loop limits to align the main body
    // iterations.
    if( should_align )
      Unimplemented();

  } else {                      // Else we have an unchanged counted loop
    if( should_peel )           // Might want to peel but do nothing else
      phase->do_peeling(this,old_new);
  }
  return true;
}


//=============================================================================
//------------------------------iteration_split--------------------------------
bool IdealLoopTree::iteration_split( PhaseIdealLoop *phase, Node_List &old_new ) {
  // Recursively iteration split nested loops
  if (_child && !_child->iteration_split(phase, old_new))
    return false;

  // Clean out prior deadwood
  DCE_loop_body();


  // Look for loop-exit tests with my 50/50 guesses from the Parsing stage.
  // Replace with a 1-in-10 exit guess.
  if (_parent /*not the root loop*/ &&
      !_irreducible &&
      // Also ignore the occasional dead backedge
      !tail()->is_top()) {
    adjust_loop_exit_prob(phase);
  }

  // Gate unrolling, RCE and peeling efforts.
  if (!_child &&                // If not an inner loop, do not split
      !_irreducible &&
      _allow_optimizations &&
      !tail()->is_top()) {     // Also ignore the occasional dead backedge
    if (!_has_call) {
        if (!iteration_split_impl(phase, old_new)) {
          return false;
        }
    } else if (policy_unswitching(phase)) {
      phase->do_unswitching(this, old_new);
    }
  }

  // Minor offset re-organization to remove loop-fallout uses of
  // trip counter when there was no major reshaping.
  phase->reorg_offsets(this);

  if (_next && !_next->iteration_split(phase, old_new))
    return false;
  return true;
}

//-------------------------------is_uncommon_trap_proj----------------------------
// Return true if proj is the form of "proj->[region->..]call_uct"
bool PhaseIdealLoop::is_uncommon_trap_proj(ProjNode* proj, Deoptimization::DeoptReason reason) {
  int path_limit = 10;
  assert(proj, "invalid argument");
  Node* out = proj;
  for (int ct = 0; ct < path_limit; ct++) {
    out = out->unique_ctrl_out();
    if (out == NULL || out->is_Root() || out->is_Start())
      return false;
    if (out->is_CallStaticJava()) {
      int req = out->as_CallStaticJava()->uncommon_trap_request();
      if (req != 0) {
        Deoptimization::DeoptReason trap_reason = Deoptimization::trap_request_reason(req);
        if (trap_reason == reason || reason == Deoptimization::Reason_none) {
           return true;
        }
      }
      return false; // don't do further after call
    }
  }
  return false;
}

//-------------------------------is_uncommon_trap_if_pattern-------------------------
// Return true  for "if(test)-> proj -> ...
//                          |
//                          V
//                      other_proj->[region->..]call_uct"
//
// "must_reason_predicate" means the uct reason must be Reason_predicate
bool PhaseIdealLoop::is_uncommon_trap_if_pattern(ProjNode *proj, Deoptimization::DeoptReason reason) {
  Node *in0 = proj->in(0);
  if (!in0->is_If()) return false;
  // Variation of a dead If node.
  if (in0->outcnt() < 2)  return false;
  IfNode* iff = in0->as_If();

  // we need "If(Conv2B(Opaque1(...)))" pattern for reason_predicate
  if (reason != Deoptimization::Reason_none) {
    if (iff->in(1)->Opcode() != Op_Conv2B ||
       iff->in(1)->in(1)->Opcode() != Op_Opaque1) {
      return false;
    }
  }

  ProjNode* other_proj = iff->proj_out(1-proj->_con)->as_Proj();
  return is_uncommon_trap_proj(other_proj, reason);
}

//-------------------------------register_control-------------------------
void PhaseIdealLoop::register_control(Node* n, IdealLoopTree *loop, Node* pred) {
  assert(n->is_CFG(), "must be control node");
  _igvn.register_new_node_with_optimizer(n);
  loop->_body.push(n);
  set_loop(n, loop);
  // When called from beautify_loops() idom is not constructed yet.
  if (_idom != NULL) {
    set_idom(n, pred, dom_depth(pred));
  }
}

//------------------------------create_new_if_for_predicate------------------------
// create a new if above the uct_if_pattern for the predicate to be promoted.
//
//          before                                after
//        ----------                           ----------
//           ctrl                                 ctrl
//            |                                     |
//            |                                     |
//            v                                     v
//           iff                                 new_iff
//          /    \                                /      \
//         /      \                              /        \
//        v        v                            v          v
//  uncommon_proj cont_proj                   if_uct     if_cont
// \      |        |                           |          |
//  \     |        |                           |          |
//   v    v        v                           |          v
//     rgn       loop                          |         iff
//      |                                      |        /     \
//      |                                      |       /       \
//      v                                      |      v         v
// uncommon_trap                               | uncommon_proj cont_proj
//                                           \  \    |           |
//                                            \  \   |           |
//                                             v  v  v           v
//                                               rgn           loop
//                                                |
//                                                |
//                                                v
//                                           uncommon_trap
//
//
// We will create a region to guard the uct call if there is no one there.
// The true projecttion (if_cont) of the new_iff is returned.
// This code is also used to clone predicates to clonned loops.
ProjNode* PhaseIdealLoop::create_new_if_for_predicate(ProjNode* cont_proj, Node* new_entry,
                                                      Deoptimization::DeoptReason reason) {
  assert(is_uncommon_trap_if_pattern(cont_proj, reason), "must be a uct if pattern!");
  IfNode* iff = cont_proj->in(0)->as_If();

  ProjNode *uncommon_proj = iff->proj_out(1 - cont_proj->_con);
  Node     *rgn   = uncommon_proj->unique_ctrl_out();
  assert(rgn->is_Region() || rgn->is_Call(), "must be a region or call uct");

  if (!rgn->is_Region()) { // create a region to guard the call
    assert(rgn->is_Call(), "must be call uct");
    CallNode* call = rgn->as_Call();
    IdealLoopTree* loop = get_loop(call);
    rgn = new (C, 1) RegionNode(1);
    rgn->add_req(uncommon_proj);
    register_control(rgn, loop, uncommon_proj);
    _igvn.hash_delete(call);
    call->set_req(0, rgn);
    // When called from beautify_loops() idom is not constructed yet.
    if (_idom != NULL) {
      set_idom(call, rgn, dom_depth(rgn));
    }
  }

  Node* entry = iff->in(0);
  if (new_entry != NULL) {
    // Clonning the predicate to new location.
    entry = new_entry;
  }
  // Create new_iff
  IdealLoopTree* lp = get_loop(entry);
  IfNode *new_iff = new (C, 2) IfNode(entry, NULL, iff->_prob, iff->_fcnt);
  register_control(new_iff, lp, entry);
  Node *if_cont = new (C, 1) IfTrueNode(new_iff);
  Node *if_uct  = new (C, 1) IfFalseNode(new_iff);
  if (cont_proj->is_IfFalse()) {
    // Swap
    Node* tmp = if_uct; if_uct = if_cont; if_cont = tmp;
  }
  register_control(if_cont, lp, new_iff);
  register_control(if_uct, get_loop(rgn), new_iff);

  // if_uct to rgn
  _igvn.hash_delete(rgn);
  rgn->add_req(if_uct);
  // When called from beautify_loops() idom is not constructed yet.
  if (_idom != NULL) {
    Node* ridom = idom(rgn);
    Node* nrdom = dom_lca(ridom, new_iff);
    set_idom(rgn, nrdom, dom_depth(rgn));
  }
  // rgn must have no phis
  assert(!rgn->as_Region()->has_phi(), "region must have no phis");

  if (new_entry == NULL) {
    // Attach if_cont to iff
    _igvn.hash_delete(iff);
    iff->set_req(0, if_cont);
    if (_idom != NULL) {
      set_idom(iff, if_cont, dom_depth(iff));
    }
  }
  return if_cont->as_Proj();
}

//--------------------------find_predicate_insertion_point-------------------
// Find a good location to insert a predicate
ProjNode* PhaseIdealLoop::find_predicate_insertion_point(Node* start_c, Deoptimization::DeoptReason reason) {
  if (start_c == NULL || !start_c->is_Proj())
    return NULL;
  if (is_uncommon_trap_if_pattern(start_c->as_Proj(), reason)) {
    return start_c->as_Proj();
  }
  return NULL;
}

//--------------------------find_predicate------------------------------------
// Find a predicate
Node* PhaseIdealLoop::find_predicate(Node* entry) {
  Node* predicate = NULL;
  if (UseLoopPredicate) {
    predicate = find_predicate_insertion_point(entry, Deoptimization::Reason_predicate);
    if (predicate != NULL) { // right pattern that can be used by loop predication
      assert(entry->in(0)->in(1)->in(1)->Opcode()==Op_Opaque1, "must be");
      return entry;
    }
  }
  return NULL;
}

//------------------------------Invariance-----------------------------------
// Helper class for loop_predication_impl to compute invariance on the fly and
// clone invariants.
class Invariance : public StackObj {
  VectorSet _visited, _invariant;
  Node_Stack _stack;
  VectorSet _clone_visited;
  Node_List _old_new; // map of old to new (clone)
  IdealLoopTree* _lpt;
  PhaseIdealLoop* _phase;

  // Helper function to set up the invariance for invariance computation
  // If n is a known invariant, set up directly. Otherwise, look up the
  // the possibility to push n onto the stack for further processing.
  void visit(Node* use, Node* n) {
    if (_lpt->is_invariant(n)) { // known invariant
      _invariant.set(n->_idx);
    } else if (!n->is_CFG()) {
      Node *n_ctrl = _phase->ctrl_or_self(n);
      Node *u_ctrl = _phase->ctrl_or_self(use); // self if use is a CFG
      if (_phase->is_dominator(n_ctrl, u_ctrl)) {
        _stack.push(n, n->in(0) == NULL ? 1 : 0);
      }
    }
  }

  // Compute invariance for "the_node" and (possibly) all its inputs recursively
  // on the fly
  void compute_invariance(Node* n) {
    assert(_visited.test(n->_idx), "must be");
    visit(n, n);
    while (_stack.is_nonempty()) {
      Node*  n = _stack.node();
      uint idx = _stack.index();
      if (idx == n->req()) { // all inputs are processed
        _stack.pop();
        // n is invariant if it's inputs are all invariant
        bool all_inputs_invariant = true;
        for (uint i = 0; i < n->req(); i++) {
          Node* in = n->in(i);
          if (in == NULL) continue;
          assert(_visited.test(in->_idx), "must have visited input");
          if (!_invariant.test(in->_idx)) { // bad guy
            all_inputs_invariant = false;
            break;
          }
        }
        if (all_inputs_invariant) {
          _invariant.set(n->_idx); // I am a invariant too
        }
      } else { // process next input
        _stack.set_index(idx + 1);
        Node* m = n->in(idx);
        if (m != NULL && !_visited.test_set(m->_idx)) {
          visit(n, m);
        }
      }
    }
  }

  // Helper function to set up _old_new map for clone_nodes.
  // If n is a known invariant, set up directly ("clone" of n == n).
  // Otherwise, push n onto the stack for real cloning.
  void clone_visit(Node* n) {
    assert(_invariant.test(n->_idx), "must be invariant");
    if (_lpt->is_invariant(n)) { // known invariant
      _old_new.map(n->_idx, n);
    } else{ // to be cloned
      assert (!n->is_CFG(), "should not see CFG here");
      _stack.push(n, n->in(0) == NULL ? 1 : 0);
    }
  }

  // Clone "n" and (possibly) all its inputs recursively
  void clone_nodes(Node* n, Node* ctrl) {
    clone_visit(n);
    while (_stack.is_nonempty()) {
      Node*  n = _stack.node();
      uint idx = _stack.index();
      if (idx == n->req()) { // all inputs processed, clone n!
        _stack.pop();
        // clone invariant node
        Node* n_cl = n->clone();
        _old_new.map(n->_idx, n_cl);
        _phase->register_new_node(n_cl, ctrl);
        for (uint i = 0; i < n->req(); i++) {
          Node* in = n_cl->in(i);
          if (in == NULL) continue;
          n_cl->set_req(i, _old_new[in->_idx]);
        }
      } else { // process next input
        _stack.set_index(idx + 1);
        Node* m = n->in(idx);
        if (m != NULL && !_clone_visited.test_set(m->_idx)) {
          clone_visit(m); // visit the input
        }
      }
    }
  }

 public:
  Invariance(Arena* area, IdealLoopTree* lpt) :
    _lpt(lpt), _phase(lpt->_phase),
    _visited(area), _invariant(area), _stack(area, 10 /* guess */),
    _clone_visited(area), _old_new(area)
  {}

  // Map old to n for invariance computation and clone
  void map_ctrl(Node* old, Node* n) {
    assert(old->is_CFG() && n->is_CFG(), "must be");
    _old_new.map(old->_idx, n); // "clone" of old is n
    _invariant.set(old->_idx);  // old is invariant
    _clone_visited.set(old->_idx);
  }

  // Driver function to compute invariance
  bool is_invariant(Node* n) {
    if (!_visited.test_set(n->_idx))
      compute_invariance(n);
    return (_invariant.test(n->_idx) != 0);
  }

  // Driver function to clone invariant
  Node* clone(Node* n, Node* ctrl) {
    assert(ctrl->is_CFG(), "must be");
    assert(_invariant.test(n->_idx), "must be an invariant");
    if (!_clone_visited.test(n->_idx))
      clone_nodes(n, ctrl);
    return _old_new[n->_idx];
  }
};

//------------------------------is_range_check_if -----------------------------------
// Returns true if the predicate of iff is in "scale*iv + offset u< load_range(ptr)" format
// Note: this function is particularly designed for loop predication. We require load_range
//       and offset to be loop invariant computed on the fly by "invar"
bool IdealLoopTree::is_range_check_if(IfNode *iff, PhaseIdealLoop *phase, Invariance& invar) const {
  if (!is_loop_exit(iff)) {
    return false;
  }
  if (!iff->in(1)->is_Bool()) {
    return false;
  }
  const BoolNode *bol = iff->in(1)->as_Bool();
  if (bol->_test._test != BoolTest::lt) {
    return false;
  }
  if (!bol->in(1)->is_Cmp()) {
    return false;
  }
  const CmpNode *cmp = bol->in(1)->as_Cmp();
  if (cmp->Opcode() != Op_CmpU ) {
    return false;
  }
  Node* range = cmp->in(2);
  if (range->Opcode() != Op_LoadRange) {
    const TypeInt* tint = phase->_igvn.type(range)->isa_int();
    if (!OptimizeFill || tint == NULL || tint->empty() || tint->_lo < 0) {
      // Allow predication on positive values that aren't LoadRanges.
      // This allows optimization of loops where the length of the
      // array is a known value and doesn't need to be loaded back
      // from the array.
      return false;
    }
  }
  if (!invar.is_invariant(range)) {
    return false;
  }
  Node *iv     = _head->as_CountedLoop()->phi();
  int   scale  = 0;
  Node *offset = NULL;
  if (!phase->is_scaled_iv_plus_offset(cmp->in(1), iv, &scale, &offset)) {
    return false;
  }
  if(offset && !invar.is_invariant(offset)) { // offset must be invariant
    return false;
  }
  return true;
}

//------------------------------rc_predicate-----------------------------------
// Create a range check predicate
//
// for (i = init; i < limit; i += stride) {
//    a[scale*i+offset]
// }
//
// Compute max(scale*i + offset) for init <= i < limit and build the predicate
// as "max(scale*i + offset) u< a.length".
//
// There are two cases for max(scale*i + offset):
// (1) stride*scale > 0
//   max(scale*i + offset) = scale*(limit-stride) + offset
// (2) stride*scale < 0
//   max(scale*i + offset) = scale*init + offset
BoolNode* PhaseIdealLoop::rc_predicate(Node* ctrl,
                                       int scale, Node* offset,
                                       Node* init, Node* limit, Node* stride,
                                       Node* range, bool upper) {
  DEBUG_ONLY(ttyLocker ttyl);
  if (TraceLoopPredicate) tty->print("rc_predicate ");

  Node* max_idx_expr  = init;
  int stride_con = stride->get_int();
  if ((stride_con > 0) == (scale > 0) == upper) {
    max_idx_expr = new (C, 3) SubINode(limit, stride);
    register_new_node(max_idx_expr, ctrl);
    if (TraceLoopPredicate) tty->print("(limit - stride) ");
  } else {
    if (TraceLoopPredicate) tty->print("init ");
  }

  if (scale != 1) {
    ConNode* con_scale = _igvn.intcon(scale);
    max_idx_expr = new (C, 3) MulINode(max_idx_expr, con_scale);
    register_new_node(max_idx_expr, ctrl);
    if (TraceLoopPredicate) tty->print("* %d ", scale);
  }

  if (offset && (!offset->is_Con() || offset->get_int() != 0)){
    max_idx_expr = new (C, 3) AddINode(max_idx_expr, offset);
    register_new_node(max_idx_expr, ctrl);
    if (TraceLoopPredicate)
      if (offset->is_Con()) tty->print("+ %d ", offset->get_int());
      else tty->print("+ offset ");
  }

  CmpUNode* cmp = new (C, 3) CmpUNode(max_idx_expr, range);
  register_new_node(cmp, ctrl);
  BoolNode* bol = new (C, 2) BoolNode(cmp, BoolTest::lt);
  register_new_node(bol, ctrl);

  if (TraceLoopPredicate) tty->print_cr("<u range");
  return bol;
}

//------------------------------ loop_predication_impl--------------------------
// Insert loop predicates for null checks and range checks
bool PhaseIdealLoop::loop_predication_impl(IdealLoopTree *loop) {
  if (!UseLoopPredicate) return false;

  if (!loop->_head->is_Loop()) {
    // Could be a simple region when irreducible loops are present.
    return false;
  }

  if (loop->_head->unique_ctrl_out()->Opcode() == Op_NeverBranch) {
    // do nothing for infinite loops
    return false;
  }

  CountedLoopNode *cl = NULL;
  if (loop->_head->is_CountedLoop()) {
    cl = loop->_head->as_CountedLoop();
    // do nothing for iteration-splitted loops
    if (!cl->is_normal_loop()) return false;
  }

  LoopNode *lpn  = loop->_head->as_Loop();
  Node* entry = lpn->in(LoopNode::EntryControl);

  ProjNode *predicate_proj = find_predicate_insertion_point(entry, Deoptimization::Reason_predicate);
  if (!predicate_proj) {
#ifndef PRODUCT
    if (TraceLoopPredicate) {
      tty->print("missing predicate:");
      loop->dump_head();
      lpn->dump(1);
    }
#endif
    return false;
  }
  ConNode* zero = _igvn.intcon(0);
  set_ctrl(zero, C->root());

  ResourceArea *area = Thread::current()->resource_area();
  Invariance invar(area, loop);

  // Create list of if-projs such that a newer proj dominates all older
  // projs in the list, and they all dominate loop->tail()
  Node_List if_proj_list(area);
  LoopNode *head  = loop->_head->as_Loop();
  Node *current_proj = loop->tail(); //start from tail
  while ( current_proj != head ) {
    if (loop == get_loop(current_proj) && // still in the loop ?
        current_proj->is_Proj()        && // is a projection  ?
        current_proj->in(0)->Opcode() == Op_If) { // is a if projection ?
      if_proj_list.push(current_proj);
    }
    current_proj = idom(current_proj);
  }

  bool hoisted = false; // true if at least one proj is promoted
  while (if_proj_list.size() > 0) {
    // Following are changed to nonnull when a predicate can be hoisted
    ProjNode* new_predicate_proj = NULL;

    ProjNode* proj = if_proj_list.pop()->as_Proj();
    IfNode*   iff  = proj->in(0)->as_If();

    if (!is_uncommon_trap_if_pattern(proj, Deoptimization::Reason_none)) {
      if (loop->is_loop_exit(iff)) {
        // stop processing the remaining projs in the list because the execution of them
        // depends on the condition of "iff" (iff->in(1)).
        break;
      } else {
        // Both arms are inside the loop. There are two cases:
        // (1) there is one backward branch. In this case, any remaining proj
        //     in the if_proj list post-dominates "iff". So, the condition of "iff"
        //     does not determine the execution the remining projs directly, and we
        //     can safely continue.
        // (2) both arms are forwarded, i.e. a diamond shape. In this case, "proj"
        //     does not dominate loop->tail(), so it can not be in the if_proj list.
        continue;
      }
    }

    Node*     test = iff->in(1);
    if (!test->is_Bool()){ //Conv2B, ...
      continue;
    }
    BoolNode* bol = test->as_Bool();
    if (invar.is_invariant(bol)) {
      // Invariant test
      new_predicate_proj = create_new_if_for_predicate(predicate_proj, NULL,
                                                       Deoptimization::Reason_predicate);
      Node* ctrl = new_predicate_proj->in(0)->as_If()->in(0);
      BoolNode* new_predicate_bol = invar.clone(bol, ctrl)->as_Bool();

      // Negate test if necessary
      bool negated = false;
      if (proj->_con != predicate_proj->_con) {
        new_predicate_bol = new (C, 2) BoolNode(new_predicate_bol->in(1), new_predicate_bol->_test.negate());
        register_new_node(new_predicate_bol, ctrl);
        negated = true;
      }
      IfNode* new_predicate_iff = new_predicate_proj->in(0)->as_If();
      _igvn.hash_delete(new_predicate_iff);
      new_predicate_iff->set_req(1, new_predicate_bol);
#ifndef PRODUCT
      if (TraceLoopPredicate) {
        tty->print("Predicate invariant if%s: %d ", negated ? " negated" : "", new_predicate_iff->_idx);
        loop->dump_head();
      } else if (TraceLoopOpts) {
        tty->print("Predicate IC ");
        loop->dump_head();
      }
#endif
    } else if (cl != NULL && loop->is_range_check_if(iff, this, invar)) {
      assert(proj->_con == predicate_proj->_con, "must match");

      // Range check for counted loops
      const Node*    cmp    = bol->in(1)->as_Cmp();
      Node*          idx    = cmp->in(1);
      assert(!invar.is_invariant(idx), "index is variant");
      assert(cmp->in(2)->Opcode() == Op_LoadRange || OptimizeFill, "must be");
      Node* rng = cmp->in(2);
      assert(invar.is_invariant(rng), "range must be invariant");
      int scale    = 1;
      Node* offset = zero;
      bool ok = is_scaled_iv_plus_offset(idx, cl->phi(), &scale, &offset);
      assert(ok, "must be index expression");

      Node* init    = cl->init_trip();
      Node* limit   = cl->limit();
      Node* stride  = cl->stride();

      // Build if's for the upper and lower bound tests.  The
      // lower_bound test will dominate the upper bound test and all
      // cloned or created nodes will use the lower bound test as
      // their declared control.
      ProjNode* lower_bound_proj = create_new_if_for_predicate(predicate_proj, NULL, Deoptimization::Reason_predicate);
      ProjNode* upper_bound_proj = create_new_if_for_predicate(predicate_proj, NULL, Deoptimization::Reason_predicate);
      assert(upper_bound_proj->in(0)->as_If()->in(0) == lower_bound_proj, "should dominate");
      Node *ctrl = lower_bound_proj->in(0)->as_If()->in(0);

      // Perform cloning to keep Invariance state correct since the
      // late schedule will place invariant things in the loop.
      rng = invar.clone(rng, ctrl);
      if (offset && offset != zero) {
        assert(invar.is_invariant(offset), "offset must be loop invariant");
        offset = invar.clone(offset, ctrl);
      }

      // Test the lower bound
      Node*  lower_bound_bol = rc_predicate(ctrl, scale, offset, init, limit, stride, rng, false);
      IfNode* lower_bound_iff = lower_bound_proj->in(0)->as_If();
      _igvn.hash_delete(lower_bound_iff);
      lower_bound_iff->set_req(1, lower_bound_bol);
      if (TraceLoopPredicate) tty->print_cr("lower bound check if: %d", lower_bound_iff->_idx);

      // Test the upper bound
      Node* upper_bound_bol = rc_predicate(ctrl, scale, offset, init, limit, stride, rng, true);
      IfNode* upper_bound_iff = upper_bound_proj->in(0)->as_If();
      _igvn.hash_delete(upper_bound_iff);
      upper_bound_iff->set_req(1, upper_bound_bol);
      if (TraceLoopPredicate) tty->print_cr("upper bound check if: %d", lower_bound_iff->_idx);

      // Fall through into rest of the clean up code which will move
      // any dependent nodes onto the upper bound test.
      new_predicate_proj = upper_bound_proj;

#ifndef PRODUCT
      if (TraceLoopOpts && !TraceLoopPredicate) {
        tty->print("Predicate RC ");
        loop->dump_head();
      }
#endif
    } else {
      // Loop variant check (for example, range check in non-counted loop)
      // with uncommon trap.
      continue;
    }
    assert(new_predicate_proj != NULL, "sanity");
    // Success - attach condition (new_predicate_bol) to predicate if
    invar.map_ctrl(proj, new_predicate_proj); // so that invariance test can be appropriate

    // Eliminate the old If in the loop body
    dominated_by( new_predicate_proj, iff, proj->_con != new_predicate_proj->_con );

    hoisted = true;
    C->set_major_progress();
  } // end while

#ifndef PRODUCT
  // report that the loop predication has been actually performed
  // for this loop
  if (TraceLoopPredicate && hoisted) {
    tty->print("Loop Predication Performed:");
    loop->dump_head();
  }
#endif

  return hoisted;
}

//------------------------------loop_predication--------------------------------
// driver routine for loop predication optimization
bool IdealLoopTree::loop_predication( PhaseIdealLoop *phase) {
  bool hoisted = false;
  // Recursively promote predicates
  if ( _child ) {
    hoisted = _child->loop_predication( phase);
  }

  // self
  if (!_irreducible && !tail()->is_top()) {
    hoisted |= phase->loop_predication_impl(this);
  }

  if ( _next ) { //sibling
    hoisted |= _next->loop_predication( phase);
  }

  return hoisted;
}


// Process all the loops in the loop tree and replace any fill
// patterns with an intrisc version.
bool PhaseIdealLoop::do_intrinsify_fill() {
  bool changed = false;
  for (LoopTreeIterator iter(_ltree_root); !iter.done(); iter.next()) {
    IdealLoopTree* lpt = iter.current();
    changed |= intrinsify_fill(lpt);
  }
  return changed;
}


// Examine an inner loop looking for a a single store of an invariant
// value in a unit stride loop,
bool PhaseIdealLoop::match_fill_loop(IdealLoopTree* lpt, Node*& store, Node*& store_value,
                                     Node*& shift, Node*& con) {
  const char* msg = NULL;
  Node* msg_node = NULL;

  store_value = NULL;
  con = NULL;
  shift = NULL;

  // Process the loop looking for stores.  If there are multiple
  // stores or extra control flow give at this point.
  CountedLoopNode* head = lpt->_head->as_CountedLoop();
  for (uint i = 0; msg == NULL && i < lpt->_body.size(); i++) {
    Node* n = lpt->_body.at(i);
    if (n->outcnt() == 0) continue; // Ignore dead
    if (n->is_Store()) {
      if (store != NULL) {
        msg = "multiple stores";
        break;
      }
      int opc = n->Opcode();
      if (opc == Op_StoreP || opc == Op_StoreN || opc == Op_StoreCM) {
        msg = "oop fills not handled";
        break;
      }
      Node* value = n->in(MemNode::ValueIn);
      if (!lpt->is_invariant(value)) {
        msg  = "variant store value";
      } else if (!_igvn.type(n->in(MemNode::Address))->isa_aryptr()) {
        msg = "not array address";
      }
      store = n;
      store_value = value;
    } else if (n->is_If() && n != head->loopexit()) {
      msg = "extra control flow";
      msg_node = n;
    }
  }

  if (store == NULL) {
    // No store in loop
    return false;
  }

  if (msg == NULL && head->stride_con() != 1) {
    // could handle negative strides too
    if (head->stride_con() < 0) {
      msg = "negative stride";
    } else {
      msg = "non-unit stride";
    }
  }

  if (msg == NULL && !store->in(MemNode::Address)->is_AddP()) {
    msg = "can't handle store address";
    msg_node = store->in(MemNode::Address);
  }

  if (msg == NULL &&
      (!store->in(MemNode::Memory)->is_Phi() ||
       store->in(MemNode::Memory)->in(LoopNode::LoopBackControl) != store)) {
    msg = "store memory isn't proper phi";
    msg_node = store->in(MemNode::Memory);
  }

  // Make sure there is an appropriate fill routine
  BasicType t = store->as_Mem()->memory_type();
  const char* fill_name;
  if (msg == NULL &&
      StubRoutines::select_fill_function(t, false, fill_name) == NULL) {
    msg = "unsupported store";
    msg_node = store;
  }

  if (msg != NULL) {
#ifndef PRODUCT
    if (TraceOptimizeFill) {
      tty->print_cr("not fill intrinsic candidate: %s", msg);
      if (msg_node != NULL) msg_node->dump();
    }
#endif
    return false;
  }

  // Make sure the address expression can be handled.  It should be
  // head->phi * elsize + con.  head->phi might have a ConvI2L.
  Node* elements[4];
  Node* conv = NULL;
  bool found_index = false;
  int count = store->in(MemNode::Address)->as_AddP()->unpack_offsets(elements, ARRAY_SIZE(elements));
  for (int e = 0; e < count; e++) {
    Node* n = elements[e];
    if (n->is_Con() && con == NULL) {
      con = n;
    } else if (n->Opcode() == Op_LShiftX && shift == NULL) {
      Node* value = n->in(1);
#ifdef _LP64
      if (value->Opcode() == Op_ConvI2L) {
        conv = value;
        value = value->in(1);
      }
#endif
      if (value != head->phi()) {
        msg = "unhandled shift in address";
      } else {
        found_index = true;
        shift = n;
        assert(type2aelembytes(store->as_Mem()->memory_type(), true) == 1 << shift->in(2)->get_int(), "scale should match");
      }
    } else if (n->Opcode() == Op_ConvI2L && conv == NULL) {
      if (n->in(1) == head->phi()) {
        found_index = true;
        conv = n;
      } else {
        msg = "unhandled input to ConvI2L";
      }
    } else if (n == head->phi()) {
      // no shift, check below for allowed cases
      found_index = true;
    } else {
      msg = "unhandled node in address";
      msg_node = n;
    }
  }

  if (count == -1) {
    msg = "malformed address expression";
    msg_node = store;
  }

  if (!found_index) {
    msg = "missing use of index";
  }

  // byte sized items won't have a shift
  if (msg == NULL && shift == NULL && t != T_BYTE && t != T_BOOLEAN) {
    msg = "can't find shift";
    msg_node = store;
  }

  if (msg != NULL) {
#ifndef PRODUCT
    if (TraceOptimizeFill) {
      tty->print_cr("not fill intrinsic: %s", msg);
      if (msg_node != NULL) msg_node->dump();
    }
#endif
    return false;
  }

  // No make sure all the other nodes in the loop can be handled
  VectorSet ok(Thread::current()->resource_area());

  // store related values are ok
  ok.set(store->_idx);
  ok.set(store->in(MemNode::Memory)->_idx);

  // Loop structure is ok
  ok.set(head->_idx);
  ok.set(head->loopexit()->_idx);
  ok.set(head->phi()->_idx);
  ok.set(head->incr()->_idx);
  ok.set(head->loopexit()->cmp_node()->_idx);
  ok.set(head->loopexit()->in(1)->_idx);

  // Address elements are ok
  if (con)   ok.set(con->_idx);
  if (shift) ok.set(shift->_idx);
  if (conv)  ok.set(conv->_idx);

  for (uint i = 0; msg == NULL && i < lpt->_body.size(); i++) {
    Node* n = lpt->_body.at(i);
    if (n->outcnt() == 0) continue; // Ignore dead
    if (ok.test(n->_idx)) continue;
    // Backedge projection is ok
    if (n->is_IfTrue() && n->in(0) == head->loopexit()) continue;
    if (!n->is_AddP()) {
      msg = "unhandled node";
      msg_node = n;
      break;
    }
  }

  // Make sure no unexpected values are used outside the loop
  for (uint i = 0; msg == NULL && i < lpt->_body.size(); i++) {
    Node* n = lpt->_body.at(i);
    // These values can be replaced with other nodes if they are used
    // outside the loop.
    if (n == store || n == head->loopexit() || n == head->incr() || n == store->in(MemNode::Memory)) continue;
    for (SimpleDUIterator iter(n); iter.has_next(); iter.next()) {
      Node* use = iter.get();
      if (!lpt->_body.contains(use)) {
        msg = "node is used outside loop";
        // lpt->_body.dump();
        msg_node = n;
        break;
      }
    }
  }

#ifdef ASSERT
  if (TraceOptimizeFill) {
    if (msg != NULL) {
      tty->print_cr("no fill intrinsic: %s", msg);
      if (msg_node != NULL) msg_node->dump();
    } else {
      tty->print_cr("fill intrinsic for:");
    }
    store->dump();
    if (Verbose) {
      lpt->_body.dump();
    }
  }
#endif

  return msg == NULL;
}



bool PhaseIdealLoop::intrinsify_fill(IdealLoopTree* lpt) {
  // Only for counted inner loops
  if (!lpt->is_counted() || !lpt->is_inner()) {
    return false;
  }

  // Must have constant stride
  CountedLoopNode* head = lpt->_head->as_CountedLoop();
  if (!head->stride_is_con() || !head->is_normal_loop()) {
    return false;
  }

  // Check that the body only contains a store of a loop invariant
  // value that is indexed by the loop phi.
  Node* store = NULL;
  Node* store_value = NULL;
  Node* shift = NULL;
  Node* offset = NULL;
  if (!match_fill_loop(lpt, store, store_value, shift, offset)) {
    return false;
  }

  // Now replace the whole loop body by a call to a fill routine that
  // covers the same region as the loop.
  Node* base = store->in(MemNode::Address)->as_AddP()->in(AddPNode::Base);

  // Build an expression for the beginning of the copy region
  Node* index = head->init_trip();
#ifdef _LP64
  index = new (C, 2) ConvI2LNode(index);
  _igvn.register_new_node_with_optimizer(index);
#endif
  if (shift != NULL) {
    // byte arrays don't require a shift but others do.
    index = new (C, 3) LShiftXNode(index, shift->in(2));
    _igvn.register_new_node_with_optimizer(index);
  }
  index = new (C, 4) AddPNode(base, base, index);
  _igvn.register_new_node_with_optimizer(index);
  Node* from = new (C, 4) AddPNode(base, index, offset);
  _igvn.register_new_node_with_optimizer(from);
  // Compute the number of elements to copy
  Node* len = new (C, 3) SubINode(head->limit(), head->init_trip());
  _igvn.register_new_node_with_optimizer(len);

  BasicType t = store->as_Mem()->memory_type();
  bool aligned = false;
  if (offset != NULL && head->init_trip()->is_Con()) {
    int element_size = type2aelembytes(t);
    aligned = (offset->find_intptr_t_type()->get_con() + head->init_trip()->get_int() * element_size) % HeapWordSize == 0;
  }

  // Build a call to the fill routine
  const char* fill_name;
  address fill = StubRoutines::select_fill_function(t, aligned, fill_name);
  assert(fill != NULL, "what?");

  // Convert float/double to int/long for fill routines
  if (t == T_FLOAT) {
    store_value = new (C, 2) MoveF2INode(store_value);
    _igvn.register_new_node_with_optimizer(store_value);
  } else if (t == T_DOUBLE) {
    store_value = new (C, 2) MoveD2LNode(store_value);
    _igvn.register_new_node_with_optimizer(store_value);
  }

  Node* mem_phi = store->in(MemNode::Memory);
  Node* result_ctrl;
  Node* result_mem;
  const TypeFunc* call_type = OptoRuntime::array_fill_Type();
  int size = call_type->domain()->cnt();
  CallLeafNode *call = new (C, size) CallLeafNoFPNode(call_type, fill,
                                                      fill_name, TypeAryPtr::get_array_body_type(t));
  call->init_req(TypeFunc::Parms+0, from);
  call->init_req(TypeFunc::Parms+1, store_value);
#ifdef _LP64
  len = new (C, 2) ConvI2LNode(len);
  _igvn.register_new_node_with_optimizer(len);
#endif
  call->init_req(TypeFunc::Parms+2, len);
#ifdef _LP64
  call->init_req(TypeFunc::Parms+3, C->top());
#endif
  call->init_req( TypeFunc::Control, head->init_control());
  call->init_req( TypeFunc::I_O    , C->top() )        ;   // does no i/o
  call->init_req( TypeFunc::Memory ,  mem_phi->in(LoopNode::EntryControl) );
  call->init_req( TypeFunc::ReturnAdr, C->start()->proj_out(TypeFunc::ReturnAdr) );
  call->init_req( TypeFunc::FramePtr, C->start()->proj_out(TypeFunc::FramePtr) );
  _igvn.register_new_node_with_optimizer(call);
  result_ctrl = new (C, 1) ProjNode(call,TypeFunc::Control);
  _igvn.register_new_node_with_optimizer(result_ctrl);
  result_mem = new (C, 1) ProjNode(call,TypeFunc::Memory);
  _igvn.register_new_node_with_optimizer(result_mem);

  // If this fill is tightly coupled to an allocation and overwrites
  // the whole body, allow it to take over the zeroing.
  AllocateNode* alloc = AllocateNode::Ideal_allocation(base, this);
  if (alloc != NULL && alloc->is_AllocateArray()) {
    Node* length = alloc->as_AllocateArray()->Ideal_length();
    if (head->limit() == length &&
        head->init_trip() == _igvn.intcon(0)) {
      if (TraceOptimizeFill) {
        tty->print_cr("Eliminated zeroing in allocation");
      }
      alloc->maybe_set_complete(&_igvn);
    } else {
#ifdef ASSERT
      if (TraceOptimizeFill) {
        tty->print_cr("filling array but bounds don't match");
        alloc->dump();
        head->init_trip()->dump();
        head->limit()->dump();
        length->dump();
      }
#endif
    }
  }

  // Redirect the old control and memory edges that are outside the loop.
  Node* exit = head->loopexit()->proj_out(0);
  // Sometimes the memory phi of the head is used as the outgoing
  // state of the loop.  It's safe in this case to replace it with the
  // result_mem.
  _igvn.replace_node(store->in(MemNode::Memory), result_mem);
  _igvn.replace_node(exit, result_ctrl);
  _igvn.replace_node(store, result_mem);
  // Any uses the increment outside of the loop become the loop limit.
  _igvn.replace_node(head->incr(), head->limit());

  // Disconnect the head from the loop.
  for (uint i = 0; i < lpt->_body.size(); i++) {
    Node* n = lpt->_body.at(i);
    _igvn.replace_node(n, C->top());
  }

  return true;
}
