type t

external stub_blob_size : unit -> int = "stub_mbuf_blob_size"
let blob_size = stub_blob_size ()

external alloc : unit -> t = "stub_mbuf_alloc"
external size : t -> int = "stub_mbuf_size" [@@noalloc]
external capacity : t -> int = "stub_mbuf_capacity" [@@noalloc]
external extend : t -> unit = "stub_mbuf_extend" [@@noalloc]
external blit : t -> int -> bytes -> int -> int -> unit = "stub_mbuf_blit"
external clear : t -> unit = "stub_mbuf_clear"
external transfer : t -> t -> unit = "stub_mbuf_transfer"

external unsafe_take_char : t -> char = "stub_mbuf_take_char" [@@noalloc]
external unsafe_take_int : t -> int = "stub_mbuf_take_int" [@@noalloc]
external unsafe_take_int8 : t -> int = "stub_mbuf_take_int8" [@@noalloc]
external unsafe_take_int32 : t -> int32 = "stub_mbuf_take_int32"
external unsafe_take_int64 : t -> int64 = "stub_mbuf_take_int64"

let take_char b =
  if size b < 1 then
    invalid_arg "Mbuf.take_char";
  unsafe_take_char b

let take_bool b =
  if size b < 1 then
    invalid_arg "Mbuf.take_char";
  if unsafe_take_int8 b = 0
  then false
  else true

let take_int b =
  if size b < 4 then
    invalid_arg "Mbuf.take_int";
  unsafe_take_int b

let take_int8 b =
  if size b < 1 then
    invalid_arg "Mbuf.take_int8";
  unsafe_take_int8 b

let take_int32 b =
  if size b < 4 then
    invalid_arg "Mbuf.take_int32";
  unsafe_take_int32 b

let take_int64 b =
  if size b < 8 then
    invalid_arg "Mbuf.take_int64";
  unsafe_take_int64 b

external take_bytes : t -> int -> bytes = "stub_mbuf_take_bytes"

let take_float b =
  if size b < 8 then
    invalid_arg "Mbuf.take_float";
  unsafe_take_int64 b |> Int64.float_of_bits

external take_mbuf : t -> int -> t = "stub_mbuf_take_mbuf"

external unsafe_put_char : t -> char -> unit = "stub_mbuf_put_int8" [@@noalloc]
external unsafe_put_int : t -> int -> unit = "stub_mbuf_put_int" [@@noalloc]
external unsafe_put_int8 : t -> int -> unit = "stub_mbuf_put_int8" [@@noalloc]
external unsafe_put_int32 : t -> int32 -> unit = "stub_mbuf_put_int32" [@@noalloc]
external unsafe_put_int64 : t -> int64 -> unit = "stub_mbuf_put_int64" [@@noalloc]
external unsafe_put_bytes : t -> bytes -> unit = "stub_mbuf_put_bytes" [@@noalloc]

let put_char b v =
  if capacity b - size b < 1 then
    extend b;
  unsafe_put_char b v

let put_bool b v =
  if capacity b - size b < 1 then
    extend b;
  unsafe_put_int8 b (if v then 1 else 0)

let put_int b v =
  if capacity b - size b < 4 then
    extend b;
  unsafe_put_int b v

let put_int8 b v =
  if capacity b - size b < 1 then
    extend b;
  unsafe_put_int8 b v

let put_int32 b v =
  if capacity b - size b < 4 then
    extend b;
  unsafe_put_int32 b v

let put_int64 b v =
  if capacity b - size b < 8 then
    extend b;
  unsafe_put_int64 b v

let put_float b v =
  if capacity b - size b < 8 then
    extend b;
  unsafe_put_int64 b (Int64.bits_of_float v)

let put_bytes b v =
  while capacity b - size b < Bytes.length v do
    extend b;
  done;
  unsafe_put_bytes b v


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
