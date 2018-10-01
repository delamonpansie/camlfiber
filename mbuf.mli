type t

val blob_size : int
external alloc : unit -> t = "stub_mbuf_alloc"
external size : t -> int = "stub_mbuf_size" [@@noalloc]
external capacity : t -> int = "stub_mbuf_capacity" [@@noalloc]
external blit : t -> int -> bytes -> int -> int -> unit = "stub_mbuf_blit"
external clear : t -> unit = "stub_mbuf_clear"
external transfer : t -> t -> unit = "stub_mbuf_transfer"

external unsafe_take_char : t -> char = "stub_mbuf_take_char" [@@noalloc]
external unsafe_take_int : t -> int = "stub_mbuf_take_int" [@@noalloc]
external unsafe_take_int8 : t -> int = "stub_mbuf_take_int8" [@@noalloc]
external unsafe_take_int32 : t -> int32 = "stub_mbuf_take_int32"
external unsafe_take_int64 : t -> int64 = "stub_mbuf_take_int64"

val take_char : t -> char
val take_bool : t -> bool
val take_int : t -> int
val take_int8 : t -> int
val take_int32 : t -> int32
val take_int64 : t -> int64
val take_float : t -> float
external take_bytes : t -> int -> bytes = "stub_mbuf_take_bytes"
external take_mbuf : t -> int -> t = "stub_mbuf_take_mbuf"

external unsafe_put_char : t -> char -> unit = "stub_mbuf_put_int8" [@@noalloc]
external unsafe_put_int8 : t -> int -> unit = "stub_mbuf_put_int8" [@@noalloc]
external unsafe_put_int : t -> int -> unit = "stub_mbuf_put_int" [@@noalloc]
external unsafe_put_int32 : t -> int32 -> unit = "stub_mbuf_put_int32" [@@noalloc]
external unsafe_put_int64 : t -> int64 -> unit = "stub_mbuf_put_int64" [@@noalloc]
external unsafe_put_bytes : t -> bytes -> unit = "stub_mbuf_put_bytes" [@@noalloc]

val put_char : t -> char -> unit
val put_bool : t -> bool -> unit
val put_int : t -> int -> unit
val put_int8 : t -> int -> unit
val put_int32 : t -> int32 -> unit
val put_int64 : t -> int64 -> unit
val put_float : t -> float -> unit
val put_bytes : t -> bytes -> unit

external blit_char : t -> int -> char -> unit = "stub_mbuf_blit_char"
external blit_bool : t -> int -> bool -> unit = "stub_mbuf_blit_char"
external blit_int : t -> int -> int -> unit = "stub_mbuf_blit_int"
external blit_int8 : t -> int -> int -> unit = "stub_mbuf_blit_char"
external blit_int32 : t -> int -> int32 -> unit = "stub_mbuf_blit_int32"
external blit_int64 : t -> int -> int64 -> unit = "stub_mbuf_blit_int64"
external blit_float : t -> int -> float -> unit = "stub_mbuf_blit_float"
external blit_bytes : t -> int -> bytes -> int -> int -> unit = "stub_mbuf_blit_bytes"

external recv : Unix.file_descr -> t -> int -> unit = "stub_mbuf_recv"
external send : Unix.file_descr -> t -> int = "stub_mbuf_send"
