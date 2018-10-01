
let is_space = function
    ' ' | '\012' | '\n' | '\r' |'\t' -> true
    | _ -> false

let wc () =
  let c = ref 0 in
  let w = ref 0 in
  let l = ref 0 in
  let rec loop prev_is_space =
    let%lwt char = Lwt_io.(read_char stdin) in
    let char_is_space = is_space char in
    if not prev_is_space && char_is_space then
      incr w;
    if char = '\n' then
      incr l;
    incr c;
    loop char_is_space
  in
  try%lwt loop true
  with End_of_file -> Printf.printf "% 7i% 7i% 7i\n" !l !w !c;
                      Lwt.return_unit

let _ =
  Lwt_main.run (wc ())
