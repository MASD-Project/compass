;;; org_element_dump.el --- Dump org-element's parse tree, one line per node  -*- lexical-binding: t -*-

;; Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
;;
;; This program is free software; you can redistribute it and/or modify it under
;; the terms of the GNU General Public License as published by the Free Software
;; Foundation; either version 3 of the License, or (at your option) any later
;; version.
;;
;; This program is distributed in the hope that it will be useful, but WITHOUT
;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
;; FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License along with
;; this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
;; Street, Fifth Floor, Boston, MA 02110-1301, USA.

;;; Commentary:

;; The oracle for the compass org parser.  It parses a file with
;; `org-element-parse-buffer' and prints every element and object in document
;; order, in the line format `compass-bench org-dump' prints for the same
;; file:
;;
;;   <indent><type> <begin> <end> <contents-begin> <contents-end> <post-blank>[ key=value]...
;;
;; Positions are byte offsets from the start of the file, also for a file
;; with CRLF line ends, which Emacs holds without them.  An absent or empty
;; contents range prints as "- -".  The indent is two spaces per level.  The
;; title of a headline and the tag of an item print as children, before the
;; contents.  An affiliated keyword prints as a line under its element:
;;
;;   <indent>affiliated key=<name>[ value=<text>]
;;
;; followed by the objects of a caption.
;;
;; Usage:
;;
;;   emacs --batch -l org_element_dump.el -- FILE
;;   emacs --batch -l org_element_dump.el -- --list LIST
;;
;; The first form prints to standard output.  LIST holds one
;; "input<TAB>output" pair per line and each dump is written to its output.

;;; Code:

(require 'cl-lib)
(require 'org)
(require 'org-element)

(defvar compass-dump--returns nil
  "For a file with CRLF line ends, the carriage returns Emacs dropped.
Element N is how many of the first N line ends of the file were CRLF.  It
is nil for a file with LF line ends, whose buffer holds every byte.")

(defun compass-dump--count-returns (file)
  "Return the value of `compass-dump--returns' for FILE."
  (with-temp-buffer
    (set-buffer-multibyte nil)
    (insert-file-contents-literally file)
    (goto-char (point-min))
    (let ((counts (list 0)))
      (while (search-forward "\n" nil t)
        (push (+ (car counts)
                 (if (eq (char-before (1- (point))) ?\r) 1 0))
              counts))
      (vconcat (nreverse counts)))))

(defun compass-dump--offset (pos)
  "Return the byte offset in the file of buffer position POS, as a string."
  (if (not pos) "-"
    (number-to-string
     (+ (1- (position-bytes pos))
        (if compass-dump--returns
            (aref compass-dump--returns (1- (line-number-at-pos pos t)))
          0)))))

(defun compass-dump--text (string)
  "Return STRING on one line: backslash, newline, return and tab escaped."
  (replace-regexp-in-string
   "[\\\n\r\t]"
   (lambda (m)
     (pcase m ("\\" "\\\\") ("\n" "\\n") ("\r" "\\r") (_ "\\t")))
   (substring-no-properties (or string "")) t t))

(defun compass-dump--name (value)
  "Return VALUE as a name, or - when it is nil or empty."
  (let ((s (cond ((null value) "")
                 ((symbolp value) (symbol-name value))
                 (t (substring-no-properties value)))))
    (if (string-empty-p s) "-" s)))

(defun compass-dump--link (link)
  "Return the properties of LINK.
A fuzzy link written as scheme:target prints the scheme as its type, as
the compass tables hold it: which schemes are link types is configuration."
  (let ((type (org-element-property :type link))
        (path (or (org-element-property :path link) ""))
        (option (org-element-property :search-option link)))
    (when (and (equal type "fuzzy")
               (let ((case-fold-search nil))
                 (string-match
                  "\\`\\([A-Za-z][-A-Za-z0-9+._]*\\):[^: \t\n]" path)))
      (setq type (match-string 1 path))
      (setq path (substring path (1+ (match-end 1)))))
    (format " type=%s format=%s path=%s" type
            (org-element-property :format link)
            (compass-dump--text (if option (concat path "::" option) path)))))

(defun compass-dump--properties (node type)
  "Return the properties the compass tables hold for NODE of TYPE."
  (pcase type
    ((or 'headline 'inlinetask)
     (let ((priority (org-element-property :priority node))
           (tags (org-element-property :tags node)))
       (format " level=%d todo=%s todo-type=%s priority=%s commented=%d tags=%s title=%s"
               (org-element-property :level node)
               (compass-dump--name (org-element-property :todo-keyword node))
               (compass-dump--name (org-element-property :todo-type node))
               (if priority (number-to-string priority) "-")
               (if (org-element-property :commentedp node) 1 0)
               (if tags (mapconcat #'substring-no-properties tags ":") "-")
               (compass-dump--text (org-element-property :raw-value node)))))
    ('keyword
     (format " key=%s value=%s"
             (downcase (org-element-property :key node))
             (compass-dump--text (org-element-property :value node))))
    ('babel-call
     (format " key=call value=%s"
             (compass-dump--text (org-element-property :value node))))
    ('node-property
     (format " key=%s value=%s"
             (upcase (org-element-property :key node))
             (compass-dump--text (org-element-property :value node))))
    ('src-block
     (format " name=%s parameters=%s"
             (compass-dump--name (org-element-property :language node))
             (compass-dump--text
              (string-join
               (delq nil (list (org-element-property :switches node)
                               (org-element-property :parameters node)))
               " "))))
    ('example-block
     (format " parameters=%s"
             (compass-dump--text
              (string-trim (or (org-element-property :switches node) "")))))
    ('export-block
     (format " name=%s"
             (downcase
              (compass-dump--name (org-element-property :type node)))))
    ('special-block
     (format " name=%s parameters=%s"
             (compass-dump--name (org-element-property :type node))
             (compass-dump--text (org-element-property :parameters node))))
    ('dynamic-block
     (format " name=%s parameters=%s"
             (compass-dump--name (org-element-property :block-name node))
             (compass-dump--text
              (string-trim (or (org-element-property :arguments node) "")))))
    ('plain-list (format " type=%s" (org-element-property :type node)))
    ('item
     (format " checkbox=%s"
             (compass-dump--name (org-element-property :checkbox node))))
    ((or 'table 'table-row)
     (format " type=%s" (org-element-property :type node)))
    ('link (compass-dump--link node))
    (_ "")))

(defun compass-dump--affiliated-key-p (key)
  "Non-nil when plist KEY names an affiliated keyword."
  (or (memq key '(:caption :name :header :results :plot))
      (string-prefix-p ":attr_" (symbol-name key))))

(defun compass-dump--affiliated (node depth)
  "Print the affiliated keywords of NODE, sorted by name, at DEPTH."
  (let ((plist (nth 1 node)) rows)
    (while plist
      (when (and (keywordp (car plist))
                 (compass-dump--affiliated-key-p (car plist))
                 (cadr plist))
        (push (cons (substring (symbol-name (car plist)) 1) (cadr plist))
              rows))
      (setq plist (cddr plist)))
    (dolist (row (sort rows (lambda (a b) (string< (car a) (car b)))))
      (let ((indent (make-string (* 2 depth) ?\s))
            (key (car row))
            (value (cdr row)))
        (cond
         ((equal key "caption")
          (dolist (caption value)
            (princ (format "%saffiliated key=caption\n" indent))
            (dolist (object (car caption))
              (unless (stringp object)
                (compass-dump--node object (1+ depth))))))
         ((equal key "results")
          (princ (format "%saffiliated key=results value=%s\n" indent
                         (compass-dump--text (car value)))))
         ((listp value)
          (dolist (v value)
            (princ (format "%saffiliated key=%s value=%s\n" indent key
                           (compass-dump--text v)))))
         (t
          (princ (format "%saffiliated key=%s value=%s\n" indent key
                         (compass-dump--text value)))))))))

(defun compass-dump--timestamps (node type)
  "Return the timestamps NODE of TYPE holds as properties, in source order."
  (sort (delq nil
              (pcase type
                ('planning
                 (list (org-element-property :closed node)
                       (org-element-property :deadline node)
                       (org-element-property :scheduled node)))
                ('clock (list (org-element-property :value node)))))
        (lambda (a b) (< (org-element-begin a) (org-element-begin b)))))

(defun compass-dump--node (node depth)
  "Print NODE and everything under it, starting at DEPTH."
  (let* ((type (org-element-type node))
         (begin (org-element-begin node))
         (end (org-element-end node))
         (cbegin (org-element-contents-begin node))
         (cend (org-element-contents-end node))
         (contents-p (and cbegin cend (< cbegin cend))))
    (princ (format "%s%s %s %s %s %s %d%s\n"
                   (make-string (* 2 depth) ?\s)
                   type
                   (compass-dump--offset begin)
                   (compass-dump--offset end)
                   (if contents-p (compass-dump--offset cbegin) "-")
                   (if contents-p (compass-dump--offset cend) "-")
                   (or (org-element-post-blank node) 0)
                   (compass-dump--properties node type)))
    (when (and (memq type org-element-all-elements)
               (not (memq type '(headline inlinetask))))
      (compass-dump--affiliated node (1+ depth)))
    (dolist (child (pcase type
                     ((or 'headline 'inlinetask)
                      (org-element-property :title node))
                     ('item (org-element-property :tag node))
                     (_ (compass-dump--timestamps node type))))
      (unless (stringp child)
        (compass-dump--node child (1+ depth))))
    (dolist (child (org-element-contents node))
      (unless (stringp child)
        (compass-dump--node child (1+ depth))))))

(defun compass-dump-file (file)
  "Print the dump of FILE to `standard-output'."
  (with-temp-buffer
    (let ((coding-system-for-read 'utf-8))
      (insert-file-contents file))
    (let ((compass-dump--returns
           (and (eq 1 (coding-system-eol-type last-coding-system-used))
                (compass-dump--count-returns file)))
          (org-inhibit-startup t)
          (org-mode-hook nil)
          (org-element-cache-persistent nil)
          (org-element--cache-self-verify nil)
          (org-resource-download-policy nil)
          (max-lisp-eval-depth 100000))
      (delay-mode-hooks (org-mode))
      (compass-dump--node (org-element-parse-buffer) 0))))

(defun compass-dump--list (list)
  "Dump every input of LIST, a file of input<TAB>output lines."
  (dolist (line (with-temp-buffer
                  (insert-file-contents list)
                  (split-string (buffer-string) "\n" t)))
    (pcase-let ((`(,input ,output) (split-string line "\t")))
      (with-temp-buffer
        (let ((standard-output (current-buffer)))
          (condition-case err
              (compass-dump-file input)
            (error (princ (format "error %S\n" err)))))
        (let ((coding-system-for-write 'utf-8-unix))
          (write-region nil nil output nil 'silent))))))

(defun compass-dump-main ()
  "Run the dump named by the command line arguments after --."
  (let ((args command-line-args-left))
    (setq command-line-args-left nil)
    (when (equal (car args) "--")
      (setq args (cdr args)))
    (set-language-environment "UTF-8")
    (pcase args
      (`("--list" ,list) (compass-dump--list list))
      (`(,file) (compass-dump-file file))
      (_ (message "usage: org_element_dump.el -- FILE | --list LIST")
         (kill-emacs 2)))))

(when noninteractive
  (compass-dump-main))

;;; org_element_dump.el ends here
