;;; The Computer Language Benchmarks Game
;;; http://shootout.alioth.debian.org/
;;; scheme version, using vectors for nodes.
;(module binary)

(define (exp2 n)
    (if (= n 0)
        1
        (if (= (modulo n 2) 0)
            (let* ((m (/ n 2))
                (x (exp2 m)))
                (* x x))
            (let* ((m (/ (- n 1) 2))
                (x (exp2 m)))
                (* 2 (* x x))))))

(define (leaf? node)
    (null? (vector-ref node 0)))

(define (make-tree item d)
    (if (= d 0)
        (vector '() item '())
        (let ((item2 (* item 2))
            (d2 (- d 1)))
            (vector (make-tree (- item2 1) d2) item (make-tree item2 d2)))))

(define (check t)
  (if (leaf? t)
      (vector-ref t 1)
      (+ (vector-ref t 1) (- (check (vector-ref t 0))
                         (check (vector-ref t 2))))))

(define (main n)
  (let* ((min-depth 4)
         (max-depth (max (+ min-depth 2) n)))
    (let ((stretch-depth (+ max-depth 1)))
        (display "stretch tree of depth ")
        (display stretch-depth)
        (display " check: ")
        (display (check (make-tree 0 stretch-depth)))
        (newline))
    (let ((long-lived-tree (make-tree 0 max-depth)))
        (do ((d 4 (+ d 2)))
            ((> d max-depth))
        (let ((iterations (exp2 (+ (- max-depth d) min-depth))))
          (display (* 2 iterations))
          (display " trees of depth ")
          (display d)
          (display " check: ")
          (display 
              (do [(i 1 (+ i 1))
                  (c 0 (+ c (+ (check (make-tree i d))
                       (check (make-tree (- 0 i) d)))))]
                  ((> i iterations) c)))
          (newline)))
      (display "long lived tree of depth ")
      (display max-depth)
      (display " check: ")
      (display (check long-lived-tree))
      (newline))))

(main 14)
;(exit)

