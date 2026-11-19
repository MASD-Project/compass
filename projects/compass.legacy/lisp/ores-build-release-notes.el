;;; ores-build-release-notes.el --- Export a sprint's release_notes.org to GitHub-flavoured Markdown. -*- lexical-binding: t; -*-

;; Copyright (C) 2026  Marco Craveiro

;; Author: Marco Craveiro <marco.craveiro@gmail.com>
;; Keywords: publish

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

;;; Commentary:
;;
;; Batch-exports <sprint-dir>/release_notes.org to
;; <sprint-dir>/release_notes.md using GitHub-flavoured Markdown
;; (ox-gfm), for use as the body of a `gh release create --notes-file`.
;;
;; Usage:
;;   emacs -Q --script projects/compass.legacy/lisp/ores-build-release-notes.el -- <sprint-dir>
;;
;; <sprint-dir> is repo-root-relative, e.g. doc/agile/versions/v0/sprint_01.
;;
;; House settings: no table of contents, no section numbers. `proj:'
;; links resolve to raw.githubusercontent.com URLs for images (so they
;; render inline in a GitHub release body) and to
;; github.com/.../blob/main/... URLs for everything else. compass
;; publishes no site, so `id:' links resolve to the github.com blob URL
;; of the linked .org file, which GitHub renders.
;;
;;; Code:
(require 'package)
(require 'org)
(require 'org-id)
(require 'ox-publish)

(setq debug-on-error nil)
(setq debug-on-quit nil)

(defvar ores/release-notes-repo-root
  (expand-file-name default-directory)
  "Repository root — batch scripts run with CMAKE_SOURCE_DIR as cwd.")

(defvar ores/release-notes-github-user "MASD-Project")
(defvar ores/release-notes-github-repo "compass")
(defvar ores/release-notes-github-branch "main")

(defun ores/release-notes--image-p (path)
  (string-match-p "\\.\\(png\\|jpe?g\\|gif\\|svg\\)$" path))

(defun ores/release-notes--raw-url (path)
  (format "https://raw.githubusercontent.com/%s/%s/%s/%s"
          ores/release-notes-github-user ores/release-notes-github-repo
          ores/release-notes-github-branch path))

(defun ores/release-notes--blob-url (path)
  (format "https://github.com/%s/%s/blob/%s/%s"
          ores/release-notes-github-user ores/release-notes-github-repo
          ores/release-notes-github-branch path))

(with-eval-after-load 'ol
  (org-link-set-parameters
   "proj"
   :export (lambda (path desc _format)
             (if (ores/release-notes--image-p path)
                 (format "![%s](%s)" (or desc (file-name-nondirectory path))
                         (ores/release-notes--raw-url path))
               (format "[%s](%s)" (or desc path)
                       (ores/release-notes--blob-url path)))))
  ;; A release note ships as a standalone GitHub release body with none
  ;; of the doc tree beside it, so a relative path would be broken for
  ;; every reader; an absolute URL to the .org source is not.
  (org-link-set-parameters
   "id"
   :export (lambda (id desc backend)
             ;; ox-gfm derives from ox-md, but link transcoding reports the
             ;; effective backend as 'md, not 'gfm -- checking for 'md
             ;; catches both (verified empirically; 'gfm alone never fires).
             (when (org-export-derived-backend-p backend 'md)
               (let* ((found (org-id-find id))
                      (file (car found)))
                 (when file
                   (let ((url (ores/release-notes--blob-url
                               (file-relative-name (expand-file-name file)
                                                   ores/release-notes-repo-root))))
                     (format "[%s](%s)" (or desc url) url))))))))

;; Suppress the TOC even if #+options: toc:t is set in-buffer — in-buffer
;; options take precedence over project/explicit :with-toc settings.
(add-hook 'org-export-before-processing-hook
          (lambda (backend)
            (when (eq backend 'gfm)
              (save-excursion
                (goto-char (point-min))
                (while (re-search-forward "\\(^#\\+options:.*\\)\\btoc:t\\b" nil t)
                  (replace-match "\\1toc:nil"))))))

(setq package-user-dir (expand-file-name "./.packages"))
(setq package-archives '(("melpa" . "https://melpa.org/packages/")
                         ("elpa" . "https://elpa.gnu.org/packages/")))
;; Keep the batch install off the user's own package cache; see the file.
(load-file (expand-file-name
            "ores-batch-packages.el"
            (file-name-directory (or load-file-name buffer-file-name))))
(package-initialize)
(unless package-archive-contents
  (package-refresh-contents))
(package-install 'ox-gfm)
(require 'ox-gfm)

(let* ((args (if (equal (car command-line-args-left) "--")
                  (cdr command-line-args-left)
                command-line-args-left))
       (sprint-dir-rel (car args)))
  (unless sprint-dir-rel
    (message "Usage: emacs -Q --script ores-build-release-notes.el -- <sprint-dir>")
    (kill-emacs 1))
  (let* ((sprint-dir (expand-file-name sprint-dir-rel ores/release-notes-repo-root))
         (org-file (expand-file-name "release_notes.org" sprint-dir)))
    (unless (file-exists-p org-file)
      (message "release_notes.org not found: %s" org-file)
      (kill-emacs 1))
    ;; Without this, id: links resolve against whatever org-id-locations
    ;; happens to already be in memory -- effectively nothing, since this
    ;; script runs under `emacs -Q` with no org-roam and no prior state.
    ;; release_notes.org routinely links out to knowledge docs and other
    ;; agile artefacts well outside its own sprint directory, so every
    ;; one of those id: links silently
    ;; became "[BROKEN LINK: <uuid>]" in the exported Markdown -- the
    ;; GitHub release body -- until this repo-wide scan runs first, same
    ;; as every other exporter.
    (setq org-id-locations-file (expand-file-name "./.org-id-locations-file"))
    ;; setq, not let: the flag is read by `bound-and-true-p' inside the
    ;; file being loaded, and a let on an undeclared symbol binds lexically
    ;; under lexical-binding, which that would not see.
    (setq ores/org-ids-library-only t)
    (load-file (expand-file-name "projects/compass.legacy/lisp/ores-org-ids.el"
                                 ores/release-notes-repo-root))
    (ores/org-id-ensure ores/release-notes-repo-root)
    (condition-case err
        (with-current-buffer (find-file-noselect org-file)
          (let ((org-export-with-toc nil)
                (org-export-with-section-numbers nil)
                (org-export-with-author nil)
                (org-export-with-sub-superscripts nil)
                (org-export-with-broken-links 'mark))
            (org-gfm-export-to-markdown))
          (message "Wrote %s"
                   (expand-file-name "release_notes.md" sprint-dir)))
      (error
       (message "Release notes export failed: %s" (error-message-string err))
       (kill-emacs 1)))))

(provide 'ores-build-release-notes)
;;; ores-build-release-notes.el ends here
