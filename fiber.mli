type 'a fiber

val create : ?name:string -> ('a -> 'b) -> 'a -> 'b fiber
external yield : unit -> unit = "stub_yield"
val resume : 'a fiber -> unit
val wake : 'a fiber -> unit
val join : 'a fiber -> 'a

external run : unit -> unit = "stub_fiber_run"
external break : unit -> unit = "stub_break"

external yield_value : unit -> 'a = "stub_yield_value"
external resume_value : 'a fiber -> 'b = "stub_yield_value"

type ev = EV_READ | EV_WRITE
external wait_io_ready : Unix.file_descr -> ev -> unit  = "stub_wait_io_ready"

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

module Unix : sig
  val sleepf : float -> unit
  val sleep : int -> unit
  val accept : ?cloexec:bool -> Unix.file_descr ->
               Unix.file_descr * Unix.sockaddr
  val connect : Unix.file_descr -> Unix.sockaddr -> unit
end
