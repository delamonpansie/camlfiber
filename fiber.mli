(** Fast native fibers for OCaml.

  Fibers are implemented by tying together {{:
   http://software.schmorp.de/pkg/libcoro.html} libcoro} and OCaml's
   GC hooks. This design allows {e allocation free} context switches
   with decent performance. For example, i7-8750H running Linux can
   perform about 13.7M fiber context switches per second. If you want
   to measure it by yourself just run [make bench]. *)

(** {2 Basic API}

   The basic API allows creation of a fiber and switching back and
   forth. It follows closely {{: https://www.lua.org/pil/9.1.html}
   Lua coroutines}.

   To give some idea of how it works, here is an example of printing
   numbers from 1 to 10 in a fiber way:
   {[
let fb = Fiber.create (fun () ->
	     for i = 1 to 10 do
	       print_int i;
	       print_newline ();
	       Fiber.yield ()
	     done) ()

let _ =
  for j = 1 to 10 do
    Fiber.resume fb
  done
]}

  More examples available in the [examples] directory.
*)

type 'a fiber
(** a fiber type. *)

val create : ('a -> 'b) -> 'a -> 'b fiber
(** [create f arg] creates a fiber. A fiber can be in one of the
   following states: sleeping, running and dead. Just after creation a
   fiber will be in sleeping state and will execute [f arg] after
   being resumed by {!resume}. Then [f] returns a fiber is dead. The
   return value of [f] can be read by calling {!join}.

   Current implementation allocates 112 pages of stack and 16 pages of
   guard zone. Graceful stack overflow detection is not (yet)
   implemented and stack overflow will result in segmentation
   violation.

   Fiber context creation is a relatively expensive process and
   therefore library caches unused contexts of dead fibers. The cache
   size is unbound, be careful when creating a lot of short living
   fibers. *)

val yield : unit -> unit
(** [yield] yields back to caller (a fiber issued
   {!resume}). Consequently, it is an error to call [yield] from
   initial context, since there is no caller. *)

val resume : 'a fiber -> unit
(** [resume fb] resumes the fiber [fb], that is: transfer execution
   context to it. Trying to resume dead fiber will raise an exception
   [Invalid_argument "Fiber.resume"]. *)

(** {2 Event loop}
    Integration with {{: http://software.schmorp.de/pkg/libev.html} libev}
    event loop.
 *)

val run : ('a -> 'b) -> 'a  -> 'b option
(** [run f arg] starts the event loop and executes [f arg] inside a
   newly created fiber. Returns [None] if {!break} is called during
   execution of [f]. *)

val break : unit -> unit
(** [break] stops event loop and exits from {!run}. *)

val wake : 'a fiber -> unit
(** [wake fb] register a wakeup for a fiber [fb]. A fiber [fb] will be resumed
   in the next iteration of the event loop. *)

val cancel_wake : 'a fiber -> unit
(** [cancel_wake fb] cancels pending wakeup for [fb]. *)

val join : 'a fiber -> 'a
(** [join fb] suspends the current fiber until [fb] is dead and returns
   the return value of it. *)

type event = READ | WRITE
val wait_io_ready : Unix.file_descr -> event -> unit
(** [wait_io_ready fd ev] suspends the current fiber until reading or
   writing from file descriptor [fd] can be performed without
   blocking.

   Please note, that the library itself doesn't put file descriptor
   into a nonblocking mode. Therefore, it's advisable to either put
   file descriptor into nonblocking mode before calling [read(2)] and
   [write(2)]or use [send(2)]and [recv(2)] with corresponding
   [flags]. *)

val sleep : float -> unit
(** [sleep s] suspends the current fiber for [s] seconds. *)

(** {2 Unsafe}

   Value passing variants of {!yield} and {!resume}. Please note, that
   it's possible to circumvent type checker by using these
   functions. It's a user’s responsibility to ensure that the type of
   value on both sides of the calls match. *)

val unsafe_yield : unit -> 'a
(** [unsafe_yield] yeilds back to a caller. See {!yield}. *)

val unsafe_resume : 'a fiber -> 'b -> unit
(** [unsafe_resume fb value] resumes [fb] and passes [value] to
   it. This [value] will be returned inside [fb] as result of
   [unsafe_yield] call. *)

(** {2 Synchronisation} *)

module Mutex : sig
  type t
  val create : unit -> t
  val lock : t -> unit
  val try_lock : t -> bool
  val unlock : t -> unit
  val with_lock : t -> (unit -> 'a) -> 'a
  val is_locked : t -> bool
end

module Condition : sig
  type t
  val create : unit -> t
  val wait : ?mutex:Mutex.t -> t -> unit
  val signal : t -> unit
  val broadcast : t -> unit
end

module MVar : sig
  type 'a t
  val create : 'a -> 'a t
  val create_empty : unit -> 'a t
  val put : 'a t -> 'a -> unit
  val take : 'a t -> 'a
  val take_available : 'a t -> 'a option
  val is_empty : 'a t -> bool
end
