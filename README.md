# itr8.tech

## Publishing

Push changes only to the local Forgejo repository (`origin`). Forgejo mirrors the
repository to GitHub, where GitHub Pages publishes the website.

Do not add a GitHub remote or push directly to GitHub: the Forgejo mirror can
overwrite changes that exist only on GitHub. Keep `main` tracking `origin/main`.

```sh
git push origin main
```
