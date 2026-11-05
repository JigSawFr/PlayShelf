// Opens one section of an NCA with LibHac (decrypted, hash tree checked,
// decompressed) and prints its size and SHA-256.
// usage: ncadump <file.nca> <prod.keys> [section=1]
using System.Security.Cryptography;
using LibHac.Common.Keys;
using LibHac.Fs;
using LibHac.FsSystem;
using LibHac.Tools.Fs;
using LibHac.Tools.FsSystem;
using LibHac.Tools.FsSystem.NcaUtils;

if (args.Length < 2) { Console.Error.WriteLine("usage: ncadump <nca> <prod.keys> [section]"); return 2; }
int section = args.Length > 2 ? int.Parse(args[2]) : 1;
var keys = new KeySet();
ExternalKeyReader.ReadKeyFile(keys, prodKeysFilename: args[1]);
using var file = new LocalStorage(args[0], FileAccess.Read);
var nca = new Nca(keys, file);
var storage = nca.OpenStorage(section, IntegrityCheckLevel.ErrorOnInvalid);
storage.GetSize(out long size).ThrowIfFailure();
using var sha = SHA256.Create();
var buf = new byte[1 << 20];
for (long off = 0; off < size;)
{
    int n = (int)Math.Min(buf.Length, size - off);
    storage.Read(off, buf.AsSpan(0, n)).ThrowIfFailure();
    sha.TransformBlock(buf, 0, n, null, 0);
    off += n;
}
sha.TransformFinalBlock(buf, 0, 0);
Console.WriteLine("{");
Console.WriteLine($"  \"size\": {size},");
Console.WriteLine($"  \"sha256\": \"{Convert.ToHexString(sha.Hash).ToLowerInvariant()}\"");
Console.WriteLine("}");
return 0;
