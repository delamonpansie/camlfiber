type t = {
    sock : Unix.file_descr;
    rbuf : Mbuf.t;
    wbuf : Mbuf.t;
  }

let make sock = { sock;
                  rbuf = Mbuf.alloc ();
                  wbuf = Mbuf.alloc (); }

let readhead = 0 (* default readahead: 1 blob *)

let rec read_char c =
  if Mbuf.size c.rbuf >= 1 then
    Mbuf.unsafe_take_char c.rbuf
  else begin
      Mbuf.recv c.sock c.rbuf readhead |> ignore;
      read_char c
    end

let rec read_bytes c size =
  if Mbuf.size c.rbuf >= size then
    Mbuf.take_bytes c.rbuf size
  else begin
      Mbuf.recv c.sock c.rbuf readhead |> ignore;
      read_bytes c size
    end

let write_char c v = Mbuf.put_char c.wbuf v
let write_bytes c v = Mbuf.put_bytes c.wbuf v
let write_mbuf _c _v = failwith "not implemented"

let refill c size =
  while Mbuf.size c.rbuf < size do
    Mbuf.recv c.sock c.rbuf (size - Mbuf.size c.rbuf) |> ignore
  done

let flush c =
  while Mbuf.size c.wbuf > 0 do
    Mbuf.send c.sock c.wbuf |> ignore
  done


let close c =
  Unix.close c.sock;
  Mbuf.clear c.rbuf;
  Mbuf.clear c.wbuf
