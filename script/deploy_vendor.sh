source script/env_deploy.sh

pushd "$SRC_ROOT"/core/server
go mod vendor
mv -T vendor "$DEPLOYMENT/vendor"
mkdir -p "$DEPLOYMENT/gen"
pushd gen
  protoc -I . --go_out="$DEPLOYMENT/gen" --protorpc_out="$DEPLOYMENT/gen" libcore.proto
popd
curl https://api.github.com/repos/sagernet/sing-box/releases/latest | jq -r '.name' > "$DEPLOYMENT/Throne.Sagernet.SingBox.Version.txt"
popd

