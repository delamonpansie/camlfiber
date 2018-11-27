[@@@warning "-52"]

let print_exc e = print_string (Printexc.to_string e);
                  print_newline ()

let f = Fiber.create (fun () ->
            Fiber.sleep 0.01;
            "ok\n") ()
let _ =
  try print_string (Fiber.join f)
  with Invalid_argument("Fiber.yield") as e -> print_exc e (* FIXME: should it be "Fiber.join" ? *)
