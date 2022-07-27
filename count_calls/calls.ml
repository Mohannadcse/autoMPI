type symb =
| Enter of (string * (string list))
| Leave of (string * (string list));;

type call =
| Call of string * (call list);;
