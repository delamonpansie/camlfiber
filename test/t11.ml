let _ =
  let handler _ = print_string "sigusr1";
                  print_newline () in
  Sys.(set_signal sigusr1 (Signal_handle handler))

let main () =
  Fiber.create (fun () ->
      Unix.kill (Unix.getpid ()) Sys.sigusr1;
      print_string "ok";
      print_newline ()) () |> Fiber.resume

let _ = Fiber.run main ()
