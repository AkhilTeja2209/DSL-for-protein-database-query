LOAD "dataset/proteins.csv"

FIND proteins
WHERE unknown_field = "Human"
AND name > 500

DISPLAY name length function
