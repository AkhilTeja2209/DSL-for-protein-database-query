LOAD "dataset/proteins.csv"
FIND proteins WHERE organism != "Human" AND length < 200 OR name = "Spike" DISPLAY proteinid name
