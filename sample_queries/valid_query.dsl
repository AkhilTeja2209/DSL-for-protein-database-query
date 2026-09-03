LOAD "dataset/proteins.csv"

FIND proteins
WHERE organism = "Human"
AND length > 300

DISPLAY name length function
