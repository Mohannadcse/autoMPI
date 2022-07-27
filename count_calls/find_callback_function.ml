(* open Lexer *)
open Calls

module Counter = CCMultiSet.Make(String);;
module StrSet = Set.Make(String);;

let count_calls calls : Counter.t =
  let filter s =
    match s with
    (* Leave works better (gives fewer candidates) than Enter *)
    | Leave (n, _) -> Some n
    | _ -> None
  in
  let called_names = CCList.filter_map filter calls in
  Counter.of_list called_names;;

(*  This is being saved for debugging reference -- it prints all the functions
    where counts2 has more calls than counts1
let print_differences counts1 counts2 =
  let differ = Counter.diff counts2 counts1 in
  Counter.iter differ (Printf.printf "%d\t%s\n");; *)

let find_candidates calls1 calls2 n : string list =
  let counts1 = count_calls calls1 in
  let counts2 = count_calls calls2 in
  let filter prev c name : string list =
    if c = n + 1 then name::prev else prev
  in
  (* Needs to be in this order if n = 0 *)
  let once = Counter.fold counts2 [] filter in
  List.filter (fun name -> Counter.count counts1 name = n) once;;

(* Removes functions called (transitively) by call from set *)
(* Consider adding "if StrSet.is_empty set' then set' else" before the last
   line *)
let rec remove_matches set call =
  let Call (name, children) = call in
  (* This does nothing if the name is not in set *)
  let set' = StrSet.remove name set in
  List.fold_left remove_matches set' children ;;


(* Consider adding "if StrSet.is_empty set' then []"
   after "if Queue.is empty queue then [] else" *)
(* We now return a (int * call) list, so that it can report the functions
   subcalled *)
let bfs top candidates : (int * call) list =
  let queue : (call Queue.t) = Queue.create () in
  Queue.add top queue;
  let rec aux (set : StrSet.t) =
    if Queue.is_empty queue then [] else
    let front = Queue.take queue in
    let Call (name, children) = front in
    if StrSet.mem name set then
      (* TODO: this only counts the first call, even if base_num (below) was 1.
         Do I actually want to remove them from the set? I'm already not adding
         the children. *)
      let set' = remove_matches set front in
      let diff = StrSet.cardinal set - StrSet.cardinal set' in
      (diff, front)::( aux set' )
    else
      begin
        List.iter (fun ch -> Queue.add ch queue) children;
        aux set
      end
  in
  aux (StrSet.of_list candidates);;

(* Prints the names of all descendants (including the call itself) *)
let rec print_all_children (Call (name, ccc)) =
  print_endline name;
  List.iter print_all_children ccc;;

(* TODO: make this a command-line parameter *)
(* The string is the name of the function you want the sub-calls of: it might
   not be the candidate selected. *)
let print_sub_calls : string option = None;;
(* Some "menu_middle_page";; mutt *)
(* Some "set_working_directory";; bash *)

let find_correct final_weighted_cands = match print_sub_calls with
  | None -> () (* Do nothing *)
  | Some func_name ->
    let pred (_, Call(c_name, _)) = (func_name = c_name) in
    try
      let (_, found) = List.find pred final_weighted_cands in
      print_all_children found
    with Not_found -> print_endline ("No such function " ^ func_name);;

let () =
  let one_unit = Util.read_calls Sys.argv.(1) in
  let two_unit = Util.read_calls Sys.argv.(2) in
  let base_num = if Array.length Sys.argv < 4 then 1
                 else int_of_string Sys.argv.(3) in
  let pass1 = find_candidates one_unit two_unit base_num in
  (* let () = Printf.printf "after pass1 length %d\n" (List.length pass1) in *)
  (* The non-empty first arg is needed for balanced calls that are not nested *)
  let nest = Util.nested two_unit [("!!Top!!", [])] [[]] in
  let pass2 = bfs nest pass1 in
  (* let () = Printf.printf "after pass2 length %d\n" (List.length pass2) in *)
  let () =
    print_endline "Candidates:";
    List.iter (fun (w, Call(n,_)) -> Printf.printf "%d\t%s\n" w n) pass2;
    print_newline ()
  in
  (* First by number and then alphabetical on call name *)
  let max_fold cur e = if e > cur then e else cur in
  (* TODO? sort and take head rather than do max? *)
  let (_, rslt) =
    List.fold_left max_fold (-1, Call("", [])) pass2
  in
  let Call (r_name, _) = rslt in
  print_endline r_name;
  print_newline ();
  find_correct pass2 ;;
