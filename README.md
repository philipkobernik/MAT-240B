# _VOICEMEMENTOS_

## 0. installation
`cd path/to/allolib_playground`
`git clone git@github.com:philipkobernik/MAT-240B.git ptk-mat240b`

## 1. process audio
`cd ptk-mat240b`

`c++ -std=c++17 -DUSE_FFTW process-audio.cpp -lfftw3 && ./a.out`

this will search in `../../corpus_755` and will process all the `.wav` files found there. create the folder if it does not exist.
on success, it wil generate 

*  `mega.meta.csv`
*  `mega.meta.no.index.csv`
*  `fileEntries.csv`

## 2. find notes
`cd ptk-mat240b`

`c++ -std=c++17 find-notes.cpp && ./a.out`

this will scan `mega.meta.csv` for notes. if successful, it will create a series of csv files that will be consumed by the allolib app.

* `note-24.csv`
* `note-24.no.index.csv`
* `note-25.csv`
* `note-25.no.index.csv`
* [... to note 39]

## 3. run knn-keyboard

`cd path/to/allolib_playground`

`./run.sh ptk9-mat240b/knn-keyboard.cpp`

once it is up, tweak params (if you like) and press the `g` key to load samples for midi notes 24-39.

press keys `1`-`9` to try to play midi notes 24-33. 

@kyber this is where it will trigger the segfault.