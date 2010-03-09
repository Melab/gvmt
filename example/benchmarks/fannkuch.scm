;; The Computer Language Benchmarks Game
;;; http://shootout.alioth.debian.org/
;;;
;; Written by Dima Dorfman, 2004
;; Slightly improved by Sven Hartrumpf, 2005-2006
;; Ever-so-slightly tweaked for MzScheme by Brent Fulgham
;; PLT-ized for v4.0 by Matthew Flatt

;#lang scheme/base
;(require scheme/cmdline)

(define (__range_helper n l)
    (if (= n 0)
        l
        (__range_helper (- n 1) (cons (- n 1) l))))
        
(define (range n)
    (__range_helper n '()))

(define (fannkuch n)
  (let ([pi (list->vector
             (range n))]
        [tmp (make-vector n)]
        [count (make-vector n)])
    (let loop ([flips 0]
               [perms 0]
               [r n])
      (when (< perms 30)
        (do ((x 0 (+ x 1)))
              ((>= x (vector-length pi)))
              (display (add1 (vector-ref pi x))))
        (newline))
      (do ((i 0 (+ i 1)))
          ((>= i r))
          (vector-set! count i (add1 i)))
      (let ((flips2 (max (count-flips pi tmp) flips)))
        (let loop2 ([r 1])
          (if (= r n)
              flips2
              (let ((perm0 (vector-ref pi 0)))
                (do ((i 0 (+ i 1)))
                      ((>= i r))
                      (vector-set! pi i (vector-ref pi (add1 i))))
                (vector-set! pi r perm0)
                (vector-set! count r (sub1 (vector-ref count r)))
                (cond
                 [(<= (vector-ref count r) 0)
                  (loop2 (add1 r))]
                 [else (loop flips2 (add1 perms) r)]))))))))

;define vector-copy! to be fair to sisc.
(define (vector-copy! dest src)
    (let ((len (vector-length src)))
    (do ((i 0 (+ i 1)))
        ((>= i len))
        (vector-set! dest i (vector-ref src i)))))

(define (count-flips pi rho)
  (vector-copy! rho pi)
  (let loop ([i 0])
    (if (= (vector-ref rho 0) 0)
        i
        (begin
          (vector-reverse-slice! rho 0 (add1 (vector-ref rho 0)))
          (loop (add1 i))))))

(define (vector-reverse-slice! v i j)
  (let loop ([i i]
             [j (sub1 j)])
    (when (> j i)
      (vector-swap! v i j)
      (loop (add1 i) (sub1 j)))))

(define (vector-swap! v i j)
  (let ((t (vector-ref v i)))
    (vector-set! v i (vector-ref v j))
    (vector-set! v j t)))

(let ((n "9"))
    (let ((pf (fannkuch (string->number n))))
              (display "Pfannkuchen(")
              (display n)
              (display ") = ")
              (display pf)
              (newline)))

