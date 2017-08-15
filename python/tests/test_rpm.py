import rpm


def test_transaction_set():
    ts = rpm.TransactionSet()
    assert ts
