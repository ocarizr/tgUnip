# Testes nos pull requests

O workflow [Firmware CI](../.github/workflows/ci.yml) executa em PRs para
`master`, em pushes para `master` e por acionamento manual. Usa um runner Ubuntu
padrão, Python 3.12 e Clang++ 18, com permissões apenas de leitura do repositório.
Não requer secrets e não grava placas nem publica resultados em serviços externos.

O job **Host tests** executa `scripts/test.ps1 -Sanitize -Simulate`, incluindo
testes C++, AddressSanitizer, testes Python de calibração/telemetria e quatro
trajetórias sintéticas. As mesmas instruções rodam localmente em PowerShell.
As bibliotecas UTFT e URTouch continuam necessárias para a compilação do sketch
para Mega, validada localmente; o job de host não compila nem testa os drivers
I²C, LCD e toque. Esses limites são complementados pelos ensaios de bancada.

## Ativação no GitHub

1. Fazer commit dos arquivos desta branch, enviá-la e abrir um PR para `master`.
   O arquivo `.github/workflows/ci.yml` deve estar incluído no commit.
2. Em **Settings → Actions → General**, confirmar que Actions está habilitado e
   que as ações oficiais `actions/checkout` e `actions/setup-python` são permitidas.
3. No PR, acompanhar **Checks → Firmware CI → Host tests**. A execução de PRs
   externos pode precisar de aprovação de um mantenedor.
4. Depois do merge, a aba **Actions → Firmware CI → Run workflow** permite uma
   execução manual; essa opção fica disponível quando o workflow está na branch
   padrão. Novos PRs executam automaticamente.
5. Opcionalmente, em **Settings → Branches**, criar proteção para `master`, marcar
   **Require status checks to pass before merging** e selecionar **Host tests**
   depois de sua primeira execução. Usar o nome exibido pelo GitHub. Essa proteção
   está disponível em repositórios públicos no Free; repositórios privados exigem
   um plano compatível. A execução do Actions independe dessa proteção.

## Plano gratuito

Segundo a documentação do GitHub consultada em 22/10/2026, runners hospedados
padrão são gratuitos para repositórios públicos. Em repositórios privados,
GitHub Free inclui **2000 minutos mensais**, com cotas próprias de armazenamento.
O workflow usa um único job, timeout de dez minutos, cancela execuções anteriores
da mesma referência e não armazena artifacts nem caches no Actions.

Consultar o consumo em **Settings → Billing and licensing** da conta ou
organização. Se o repositório for privado, pode-se configurar um orçamento com
interrupção no limite para controlar eventual consumo além da franquia.

Fontes oficiais: [cobrança do Actions](https://docs.github.com/en/billing/concepts/product-billing/github-actions),
[franquias por plano](https://docs.github.com/en/billing/reference/product-usage-included)
e [disponibilidade de proteção de branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches).
