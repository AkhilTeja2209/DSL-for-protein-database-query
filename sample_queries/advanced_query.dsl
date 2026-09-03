// Every clause the language supports, over the bundled CSV.

LOAD "dataset/proteins.csv"

// AND binds tighter than OR, so this reads as
//   (organism = "Human" AND length > 500) OR (organism = "Chicken")
FIND proteins
WHERE organism = "Human" AND length > 500
OR organism = "Chicken"
SORT BY length DESC
DISPLAY proteinid name organism length

// SEARCH is a free-text scan over every attribute, which doubles as a
// naive sequence-motif search.
FIND proteins SEARCH "MVLSPADK" DISPLAY name length

// COUNT reports the size of the result set instead of listing it.
FIND proteins WHERE organism = "Human" COUNT
