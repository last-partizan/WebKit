function assert(a, e) {
    if (a !== e)
        throw new Error("Bad");
}

function valueSub() {
    let sum = 0;
    do {
        // We trigger the JIT compilation of valueSub
        // so Date.now() will have SpecNone as result
        for (let i = 0; i < testLoopCount; i++)
            sum++;

        sum += 0.5;
    } while (Date.now() - sum  < 0);

    assert(sum, testLoopCount + 0.5);
}
noInline(valueSub);

valueSub();

