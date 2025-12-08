;; stft_istft.scm
;;

(load "stdlib.scm")

(def snd   (readwav "data/Vox.wav"))
(def sr    (lindex snd 0))
(def chans (lindex snd 1))

(def N   2048)
(def hop (/ N 8))     ;; 12.5% hop

(def ch (lindex chans 0))

(def specs (stft ch N hop))
(def recon (istft specs N hop))
(writewav "Vox_stft_istft_test.wav" sr (list recon))

(def ratio 2)
(def outhop (* hop ratio))
(def stretch (istft specs N outhop))
(writewav "Vox_stft_istft_2x.wav" sr (list stretch))

(= ch (resample ch 2))
(writewav "Vox_resampled_2down.wav" sr (list ch))


;; eof
