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
  include module type of Unix

  external sleepf : float -> unit = "stub_fiber_sleep"

end with type error = Unix.error
     and type process_status = Unix.process_status
     and type wait_flag = Unix.wait_flag
     and type file_descr = Unix.file_descr
     and type file_perm = int
     and type open_flag = Unix.open_flag
     and type seek_command = Unix.seek_command
     and type file_kind = Unix.file_kind
     and type stats = Unix.stats
     and type access_permission = Unix.access_permission
     and type dir_handle = Unix.dir_handle
     and type lock_command = Unix.lock_command
     and type sigprocmask_command = Unix.sigprocmask_command
     and type process_times = Unix.process_times
     and type tm = Unix.tm
     and type interval_timer = Unix.interval_timer
     and type interval_timer_status = Unix.interval_timer_status
     and type passwd_entry = Unix.passwd_entry
     and type group_entry = Unix.group_entry
     and type inet_addr = Unix.inet_addr
     and type socket_domain = Unix.socket_domain
     and type socket_type = Unix.socket_type
     and type sockaddr = Unix.sockaddr
     and type shutdown_command = Unix.shutdown_command
     and type msg_flag = Unix.msg_flag
     and type socket_bool_option = Unix.socket_bool_option
     and type socket_int_option = Unix.socket_int_option
     and type socket_optint_option = Unix.socket_optint_option
     and type socket_float_option = Unix.socket_float_option
     and type host_entry = Unix.host_entry
     and type protocol_entry = Unix.protocol_entry
     and type service_entry = Unix.service_entry
     and type addr_info = Unix.addr_info
     and type getaddrinfo_option = Unix.getaddrinfo_option
     and type name_info = Unix.name_info
     and type getnameinfo_option = Unix.getnameinfo_option
     and type terminal_io = Unix.terminal_io
     and type setattr_when = Unix.setattr_when
     and type flush_queue = Unix.flush_queue
     and type flow_action = Unix.flow_action

