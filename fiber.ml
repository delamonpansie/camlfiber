type id = int
type fqueue = id Queue.t

type 'a fiber = { mutable id: id;
                  mutable result: 'a option;
                  joinq: fqueue;
                }

external self_id : unit -> int = "stub_fiber_id" [@@noalloc]

external stub_create : string -> ('a -> unit) -> 'a -> int = "stub_fiber_create"
external run : unit -> unit = "stub_fiber_run"
external break : unit -> unit = "stub_break"

external wake_id : int -> unit = "stub_wake"
external yield : unit -> unit = "stub_yield"
external resume : 'a fiber -> unit = "stub_resume"

external yield_value : unit -> 'a = "stub_yield_value"
external resume_value : 'a fiber -> 'b = "stub_yield_value"

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

let create ?(name="") f v =
  let fiber = { id = -1; result = None; joinq = FQueue.create (); } in
  let wrap v =
    fiber.result <- Some (f v);
    FQueue.wake fiber.joinq
  in
  fiber.id <- stub_create name wrap v;
  fiber

let rec join f =
  match f.result with
    Some v -> v
  | None ->
     FQueue.yield f.joinq;
     join f

type ev = EV_READ | EV_WRITE
external wait_io_ready : Unix.file_descr -> ev -> unit  = "stub_wait_io_ready"

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

module Unix = struct
  include Unix

  external sleepf : float -> unit = "stub_fiber_sleep"
  let sleep s = sleepf (float s)

  let accept ?cloexec sock =
    wait_io_ready sock EV_READ;
    accept ?cloexec sock

  let connect sock addr =
    (try connect sock addr
     with Unix_error (EINPROGRESS, _, _)
        | Unix_error (EAGAIN, _, _)
        | Unix_error (EWOULDBLOCK, _, _) -> ());
    wait_io_ready sock EV_WRITE;
    match getsockopt_error sock with
      None -> ()
    | Some error -> raise (Unix_error (error, "connect", ""))
end



let _ =
  assert(Sys.int_size == 63)
