[@@@warning "-52"]

let print_exc e = print_string (Printexc.to_string e);
                  print_newline ()

let _ =
  let r = ref (Fiber.create (fun () -> ()) ()) (* dummy placeholder *) in
  let f = Fiber.create (fun () ->
              try Fiber.resume !r (* fiber recursion is not allowed *)
              with Invalid_argument("Fiber.resume") as e -> print_exc e) () in
  r := f;
  Fiber.resume f
