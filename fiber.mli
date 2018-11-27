type 'a fiber

val create : ('a -> 'b) -> 'a -> 'b fiber
external yield : unit -> unit = "stub_yield"
val resume : 'a fiber -> unit

val wake : 'a fiber -> unit
val cancel_wake : 'a fiber -> unit
val join : 'a fiber -> 'a

val run : ('a -> 'b) -> 'a  -> 'b option
external break : unit -> unit = "stub_break"

external unsafe_yield : unit -> 'a = "stub_unsafe_yield"
external unsafe_resume : 'a fiber -> 'b -> unit = "stub_unsafe_resume"

type ev = EV_READ | EV_WRITE
external wait_io_ready : Unix.file_descr -> ev -> unit  = "stub_wait_io_ready"

external sleep : float -> unit = "stub_fiber_sleep"

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
