(*
 * Copyright (C) 2018 Yuriy Vostrikov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *)

type id = int
type fqueue = id Queue.t

type 'a fiber = { mutable id: id;
                  mutable result: 'a option;
                  joinq: fqueue;
                }

external self_id : unit -> int = "stub_fiber_id" [@@noalloc]

external stub_create : ('a -> unit) -> 'a -> int = "stub_fiber_create"
external break : unit -> unit = "stub_break"

external wake_id : int -> unit = "stub_wake"
external cancel_wake_id : int -> unit = "stub_cancel_wake"

external yield : unit -> unit = "stub_yield"
external resume : 'a fiber -> unit = "stub_resume"

external unsafe_yield : unit -> 'a = "stub_unsafe_yield"
external unsafe_resume : 'a fiber -> 'b -> unit = "stub_unsafe_resume"

external sleep : float -> unit = "stub_fiber_sleep"

module FQueue = struct
  include Queue
  let yield q =
    Queue.push (self_id ()) q;
    yield ()

  let wake q =
    Queue.iter wake_id q;
    Queue.clear q
end

let wake f =
  wake_id f.id

let cancel_wake f =
  cancel_wake_id f.id

let create f v =
  let fiber = { id = -1; result = None; joinq = FQueue.create (); } in
  let wrap v =
    fiber.result <- Some (f v);
    FQueue.wake fiber.joinq
  in
  fiber.id <- stub_create wrap v;
  fiber

let rec join f =
  match f.result with
    Some v -> v
  | None ->
     FQueue.yield f.joinq;
     join f

external stub_run : 'a fiber -> unit = "stub_fiber_run"

let run g a =
  let f = create (fun () ->
              let v = g a in
              break ();
              v) () in
  wake f;
  stub_run f;
  f.result

type event = READ | WRITE
external wait_io_ready : Unix.file_descr -> event -> unit  = "stub_wait_io_ready"

module Mutex = struct
  type t = { mutable locked: bool;
             waitq: fqueue }

  let create () = { locked = false;
                    waitq = FQueue.create () }
  let rec lock l =
    match l.locked with
      false -> l.locked <- true
    | true -> FQueue.yield l.waitq;
              lock l

  let try_lock l =
    match l.locked with
      false -> l.locked <- true;
               true
    | true -> false

  let unlock l =
    l.locked <- false;
    FQueue.wake l.waitq

  let with_lock l f =
    try
      lock l;
      let r = f () in
      unlock l;
      r
    with e ->
      unlock l;
      raise e

  let is_locked l = l.locked
end

module Condition = struct
  type t = fqueue

  let create () = FQueue.create ()

  let wait ?mutex c =
    let option_iter f = function Some v -> f v
                               | None -> () in
    option_iter Mutex.unlock mutex;
    FQueue.yield c;
    option_iter Mutex.lock mutex

  let signal c =
    try wake_id (Queue.pop c)
    with FQueue.Empty -> ()

  let broadcast c =
    FQueue.wake c
end

module MVar = struct
  type 'a t = { mutable value : 'a option;
                mutable put_wait : fqueue;
                mutable take_wait : fqueue }

  let make value = { value;
                     put_wait = FQueue.create ();
                     take_wait = FQueue.create (); }

  let create value = make (Some value)
  let create_empty () = make None

  let rec put v value =
    match v.value with
      None ->
       v.value <- Some value;
       FQueue.wake v.take_wait
    | Some _ ->
       FQueue.yield v.put_wait;
       put v value

  let rec take v =
    match v.value with
      Some value ->
       v.value <- None;
       FQueue.wake v.put_wait;
       value
    | None ->
       FQueue.yield v.take_wait;
       take v

  let take_available ({value; _} as v) =
    if value <> None then begin
      v.value <- None;
      FQueue.wake v.put_wait;
    end;
    value

  let is_empty v = v.value == None
end

let _ =
  assert(Sys.int_size == 63)
