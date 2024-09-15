@echo off

pushd "%~dp0"

call pnpm install
call pnpm run generate-fonts

popd
